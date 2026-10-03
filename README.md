# Dual servo serial adapter

PlatformIO / Arduino firmware for a classic ESP32-WROOM development board. It
duplicates commands from a serial tuner to two RS-232 servo controllers and
returns replies from the servo selected by a switch.

The board uses four serial links:

| Role | ESP32 connection | Baud / format |
| --- | --- | --- |
| Programming and debug console | Onboard USB bridge, UART0 on GPIO1/GPIO3 | 115200, 8N1 |
| Tuner | Software serial: GPIO21 RX, GPIO22 TX | 38400, 8N1 |
| Servo A | UART1: GPIO16 RX, GPIO17 TX | 38400, 8N1 |
| Servo B | UART2: GPIO19 RX, GPIO18 TX | 38400, 8N1 |

The tuner link carries every command to both servos. The reply selector only
chooses which servo reply returns to the tuner.

## Wiring

Use the GPIO labels on the ESP32 board, not physical header positions.

### Tuner USB-TTL adapter

Use a **3.3 V TTL** adapter. Do not connect an RS-232 adapter directly to the
ESP32.

| USB-TTL adapter | ESP32 | ESP32 header contact |
| --- | --- | ---: |
| TXD | GPIO21 (ESP32 RX) | 33 |
| RXD | GPIO22 (ESP32 TX) | 36 |
| GND | GND | 32, 38, or 14 |
| DTR / RTS / CTS | Leave disconnected | - |
| VCC | Leave disconnected | - |

The tuner software opens this adapter's COM port at 38400 baud, 8N1. Because
its DTR and RTS pins are not wired to the ESP32, opening that port cannot
operate the board's EN or GPIO0 reset circuitry.

### Servo links

| ESP32 | MAX3233E | Direction |
| --- | --- | --- |
| GPIO17, contact 28 | T1IN, pin 4 | ESP32 TX to Servo A |
| GPIO16, contact 27 | R1OUT, pin 6 | Servo A RX to ESP32 |
| GPIO18, contact 30 | T2IN, pin 3 | ESP32 TX to Servo B |
| GPIO19, contact 31 | R2OUT, pin 1 | Servo B RX to ESP32 |
| GPIO25, contact 9 | Switch contact; other switch contact to GND | Reply selector |
| 3V3, contact 1 | VCC, pin 9 | MAX3233E power |
| GND | GND, pin 18 | Common reference |

Connect `T1OUT` to Servo A's RS-232 RX and Servo A's RS-232 TX to `R1IN`.
Connect `T2OUT` to Servo B's RS-232 RX and Servo B's RS-232 TX to `R2IN`.

Tie MAX3233E `FORCEON` pin 5 and active-low `FORCEOFF` pin 10 high to 3.3 V.

| Switch state | Reply returned to the tuner |
| --- | --- |
| GPIO25 connected to GND | Servo A |
| GPIO25 open | Servo B |

## Build, upload, and debug

Build and upload through the ESP32 board's normal USB connection:

```powershell
pio run --target upload --upload-port COM5
```

Replace `COM5` with the board's COM port. The default build is `esp32dev`.

Open a serial terminal on the board's USB COM port at 115200 baud to see the
startup message and a bridge counter line once per second. Configure the
terminal with DTR and RTS disabled when possible. The tuner software uses the
external USB-TTL adapter's COM port, so the two programs do not compete for a
port.

Example debug output:

```text
dual-servo bridge ready | tuner RX=21 TX=22 at 38400 8N1
bridge 1000 ms | tuner rx=0 tx=0 | A rx=0 tx=0 | B rx=0 tx=0 | discarded=0 failed=0
```

`tuner rx` and `tuner tx` count bytes entering and leaving the GPIO21/GPIO22
link. `A` and `B` counters show traffic at the two MAX3233E channels.
