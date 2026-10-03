# Dual servo serial adapter

PlatformIO / Arduino firmware for a classic ESP32-WROOM development board with
19 header pins per side and an onboard USB-to-serial bridge. The project uses
the generic `esp32dev` target, assuming the usual 4 MB flash, without PSRAM.
Header count alone does not identify a board; this target is for the original
ESP32, not ESP32-S2/S3/C3 or ESP32-WROVER modules.

The PC sends bytes over USB at **38400 baud, 8N1, no flow control**. Every byte
is forwarded to both servo controllers through the two MAX3233E channels.
Only the selected servo's reply is returned to the PC; replies from the other
servo are continuously discarded. Both servos must use the same serial format
and accept the same commands/addressing for duplicated commands to work.

## Debug a drive that is not detected

The default build is now **`esp32debug`**, which adds a Wi-Fi traffic viewer.
The USB serial connection remains dedicated to the tuning software: do not
open a serial monitor on that COM port while tuning.

1. Upload the debug build: `pio run -e esp32debug --target upload --upload-port COM5`
   (replace COM5 with the ESP32 port). In PlatformIO's Project Tasks you can
   also select **esp32debug > General > Upload**.
2. On your phone or another Wi-Fi-capable device, join **DualServo-Debug** with
   password **servo-debug**. Stay connected even if it says "no internet".
   A phone lets the tuning PC keep its usual network connection.
3. Open **http://192.168.4.1/** in a browser. No router, internet connection,
   extra USB adapter, or extra wiring is required.
4. Check the selected reply shown on the page. With communication idle, change
   the switch and verify it changes between A and B. Open switch selects B.
5. Start a drive search in the tuning software. Watch the counters and hex
   traffic, then stop the search and click **Pause display**, **Save capture**.
   The saved text is the latest displayed snapshot; pausing freezes the display,
   not firmware capture. The raw snapshot is also at `http://192.168.4.1/capture.txt`.

| Observation during a search | What to investigate next |
| --- | --- |
| PC RX remains zero | Wrong COM port, PC software not sending, reset on opening the port, or USB/UART0 receive path. Check uptime and PC UART errors. |
| PC RX and both TX counts rise, but both drive RX counts stay zero | Commands reached the UART drivers. Check TX/RX wiring, MAX3233E supply and enable pins, common ground, drive power, and drive serial settings. |
| Only A RX rises, selected reply is B | A's replies are intentionally discarded. Select A while idle and retry. The reverse applies to B. |
| Selected drive RX and PC TX rise, but detection still fails | Save the hex capture. Check baud/framing, protocol contents and response timing against a known-working direct adapter connection. |
| Framing errors or break events rise | Check serial settings, inversion/RS-232 level conversion, signal integrity, and wiring. A break can also be intentionally sent by software. |
| RX buffer full / FIFO overflow rises | Received bytes may have been lost. Save the capture and note the traffic load. |
| Uptime restarts / hotspot disappears when tuning software opens the COM port | The board may be resetting via DTR/RTS or power instability. Wait for the page to reconnect, then retry a search without reopening the port if possible. |
| Bridge heartbeat age grows while page still responds | The forwarding loop is not reaching its selector update; sustained input or a blocked UART write needs investigation. |

**Check the labels carefully:** in `esp32_pinout.png`, GPIO17 is labeled TX2
and GPIO16 is labeled RX2. This firmware follows your original assignments:
**GPIO17 receives R1OUT; GPIO16 drives T1IN**. The ESP32 permits remapping, so
those printed default UART labels do not describe this firmware's assignments.

The viewer shows timestamps in microseconds since boot (wrapping after about
71 minutes), byte counts since boot, raw reset reason from ESP-IDF, UART error
event counts, and the newest 256 traffic rows. Nearby bytes on the same route
are grouped up to 32 bytes per row, with a new row after a gap over 2 ms. These
are software observation timestamps after UART writes, not wire-level timing
measurements or decoded protocol packet boundaries. Counters are 32-bit.
TX counts confirm acceptance by the UART driver, not electrical transmission
or drive receipt. An empty error table does not prove correct baud or wiring;
for example, parity errors cannot validate parity while configured for 8N1.

Old capture rows are overwritten when full; the page reports how many. That
counter indicates lost diagnostic history, separately from UART overflow.
There is no full-session recorder. Save shortly after the failed search.
Every ESP32 reset clears the capture and counters. Wi-Fi serving runs on core
0 separately from the Arduino forwarding loop; capture uses fixed memory and
a short lock. Wi-Fi still adds CPU/power load, so compare with the baseline if
the behavior changes under diagnostics. The hotspot starts whenever this debug
firmware boots and exposes read-only traffic to clients with its password.

To build/upload the original behavior with Wi-Fi disabled:

```powershell
pio run -e esp32dev --target upload --upload-port COM5
```

This instrumentation does not establish or fix the cause of failed drive
detection by itself. Hardware captures and comparison with a working direct
USB-to-RS232 connection are the next evidence needed.

### Wi-Fi disappears as soon as the tuning software connects

First distinguish a firmware crash from USB control lines holding the chip in
reset or selecting its ROM download mode. On many ESP32 development boards,
the onboard USB bridge drives EN and GPIO0 through DTR/RTS. Opening a port can
change these lines before any command bytes arrive. The PlatformIO settings
`monitor_dtr = 0` and `monitor_rts = 0` apply only to PlatformIO's monitor;
they do not control the tuning software. See
[Espressif's reset/boot-mode explanation](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html).

1. When the failure occurs, close the tuning software completely without
   unplugging USB. Check whether the hotspot comes back. If not, with the
   application closed, briefly press EN/RESET (leave BOOT released) and check
   again. Recovery here is evidence worth recording, not proof of a cause.
2. With the tuning software closed and the debug page working, run the passive
   probe below. It explicitly requests DTR and RTS off before opening the
   port and **never sends bytes**, so no servo commands are generated.
3. If the hotspot stays up with the probe but disappears with the tuning
   software, compare that software's DTR/RTS/flow-control settings. Set both
   DTR and RTS off if the software offers that choice. This points toward its
   port-opening sequence or transmitted traffic; it does not alone distinguish
   them. Some USB drivers can still briefly glitch these lines when opening.

From PowerShell in the project folder (replace COM8 with the actual ESP32 port):

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/serial_probe.py --port COM8
```

For boot evidence, use 115200 baud, then briefly press EN/RESET while the probe
is already open and BOOT is released:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/serial_probe.py --port COM8 --baud 115200 --seconds 60
```

The ROM's `DOWNLOAD_BOOT` / `waiting for download` messages indicate bootloader
mode. `SPI_FAST_FLASH_BOOT` indicates normal flash boot. A `Guru Meditation`
or watchdog/brownout message is evidence of a different failure. Application
traffic runs at 38400, so it will not decode correctly in the 115200 boot probe.
The probe owns the port exclusively and cannot run beside the tuning software
on the same COM port, nor recover the tuning software's previous line states.
Manual reset or power cycling also replaces the reset evidence from the
original failure. Do not interpret the subsequent reset reason as the cause
of the earlier failure.

If the tuning software holds EN low or selects download mode, a firmware
change cannot override those hardware signals. Confirm the board's reset
circuit and control-line behavior before modifying hardware.

## Build and upload

1. Open this folder in VS Code with the PlatformIO IDE extension installed.
2. Connect the ESP32 using a USB data cable. Close the servo tuning software
   and any serial monitor so the COM port is available.
3. Click **PlatformIO: Build** (checkmark), then **PlatformIO: Upload** (arrow).
   The first build may download the ESP32 tools and Arduino framework.
4. If port detection fails, run `pio device list`, then set `upload_port` in
   `platformio.ini` to the ESP32's actual COM port.
5. If upload stays at `Connecting...`, hold **BOOT**, start upload, and release
   BOOT once writing starts. If necessary, tap **EN/RESET** while holding BOOT.
6. After upload, close the PlatformIO serial monitor and select that same COM
   port in the tuning software at **38400, 8N1, no flow control**.

Equivalent commands from a PlatformIO terminal in this folder:

```powershell
pio run
pio device list
pio run --target upload --upload-port COM5
```

Replace `COM5` with the board's port. Uploading uses 115200 baud; the running
adapter uses 38400 baud. Only one application can own the COM port at a time.
There is no application startup banner or debug text on USB. In the default
debug build, diagnostics are available through the separate Wi-Fi viewer.

## Connections

Use the **GPIO labels**, not header position numbers. Pins marked RX0/TX0
(GPIO3/GPIO1) are used by the onboard USB bridge and should stay free.

| ESP32 | MAX3233E / switch | Direction |
| --- | --- | --- |
| GPIO17 | R1OUT | Servo A receive into ESP32 |
| GPIO16 | T1IN | ESP32 transmit to Servo A |
| GPIO19 | R2OUT | Servo B receive into ESP32 |
| GPIO18 | T2IN | ESP32 transmit to Servo B |
| GPIO25 | Switch contact; other contact to GND | Reply selector |
| 3V3 | VCC | MAX3233E supply |
| GND | GND and servo serial signal grounds | Common reference |

On the RS-232 side, connect `T1OUT` to Servo A's RS-232 RX and its RS-232 TX to
`R1IN`. Connect `T2OUT` to Servo B's RS-232 RX and its RS-232 TX to `R2IN`.
Check the servo connector documentation for its actual pinout. RS-232 signals
must pass through the transceiver; they must not connect directly to ESP32 GPIO.

The **MAX3233E uses 3.3 V** (3.0-3.6 V), unlike the 5 V MAX3235E. For continuous
operation, tie `FORCEON` and active-low `FORCEOFF` high to its 3.3 V supply.
Follow the manufacturer's supply bypassing and circuit instructions; a bare
chip still needs its supply bypass capacitor. See the
[MAX3233E/MAX3235E datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max3233e-max3235e.pdf).

| Switch state | Reply sent to PC |
| --- | --- |
| GPIO25 connected to GND | Servo A |
| GPIO25 open | Servo B |

The switch selects **replies only**. Writes, configuration changes, and any
motion commands go to **both** servos in either switch position.

## Behavior and bench check

This preserves the supplied byte-forwarding loop. Each UART gets a 1024-byte
receive buffer, configured before `begin()`. It does not parse protocol frames,
debounce the switch, or synchronize switching to transactions. Stop polling and
let replies finish before changing the selector, then resume communication.
Switching during a reply can truncate or mix responses. Buffers are finite;
sustained overload is not protected by flow control.

The classic ESP32 may emit ROM boot messages on UART0 during reset, and PC
software that toggles DTR/RTS may reset the board. The firmware disables
application debug output, but cannot suppress the ROM's early boot output.
Let startup finish and discard pending PC input before beginning a transaction.

Before connecting servos, a simple logic-side loopback check is possible with
the transceiver disconnected from these GPIOs:

1. Connect GPIO16 to GPIO17 and GPIO18 to GPIO19.
2. Open a serial terminal at 38400 8N1 with local echo disabled.
3. Send a short byte sequence. It should return once, in either switch position.
4. Remove A's loopback: A selected should return nothing, B selected should echo.
5. Restore A, remove B, and verify the opposite. Remove all test jumpers before
   reconnecting the transceiver.

This checks basic routing only. Validate the actual RS-232 wiring and servo
protocol with the hardware connected before relying on the adapter.

Board configuration reference:
[PlatformIO ESP32 Dev Module](https://docs.platformio.org/en/stable/boards/espressif32/esp32dev.html).

## Development checks

Both builds: `pio run -e esp32dev -e esp32debug`.

Host tests run the actual forwarding loop with simulated UARTs, and test the
fixed-size capture buffer independently of hardware. From a Visual Studio
Developer PowerShell in this folder:

```powershell
New-Item -ItemType Directory -Force .pio | Out-Null
cl /nologo /EHsc /std:c++14 /W4 /Itests/fakes /Isrc tests/test_bridge.cpp /Fe:.pio/test-bridge.exe /Fo:.pio/test-bridge.obj
& ./.pio/test-bridge.exe
cl /nologo /EHsc /std:c++14 /W4 /Isrc tests/test_capture.cpp /Fe:.pio/test-capture.exe /Fo:.pio/test-capture.obj
& ./.pio/test-capture.exe
node tests/test_debug_page.cjs
python -m unittest discover -s tests -p "test_*.py"
```

The host checks do not verify real UART timing, radio operation, transceiver
wiring, or successful drive discovery; those require the connected hardware.
