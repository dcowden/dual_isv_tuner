"""Open the ESP32 COM port with DTR/RTS off and receive only; never send bytes."""
import argparse
import math
import sys
import time


def open_passive_port(serial_module, name, baud):
    # Construct CLOSED: Serial(name, ...) would assert its default control-line
    # states before we could change them. Windows/drivers can still glitch on open.
    port = serial_module.Serial(
        port=None, baudrate=baud, bytesize=8, parity="N", stopbits=1,
        timeout=0.2, xonxoff=False, rtscts=False, dsrdtr=False,
    )
    port.dtr = False
    port.rts = False
    port.port = name
    port.open()
    return port


def observe(port, seconds, emit=print, clock=time.monotonic):
    started = clock()
    while clock() - started < seconds:
        data = port.read(256)
        if data:
            # repr prevents received terminal control characters from executing.
            emit(f"{clock() - started:7.3f}s RX {data.hex(' ').upper()}  {data!r}")


def positive_seconds(value):
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise argparse.ArgumentTypeError("duration must be finite and greater than zero")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ESP32 COM port, e.g. COM8")
    parser.add_argument("--baud", type=int, choices=(38400, 115200), default=38400,
                        help="38400 for the running bridge; 115200 for ROM boot messages")
    parser.add_argument("--seconds", type=positive_seconds, default=30,
                        help="observation duration, default 30 seconds")
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        print("pyserial is required. Use PlatformIO's Python, or install with: "
              "python -m pip install pyserial", file=sys.stderr)
        return 1
    print(f"Opening {args.port} at {args.baud} 8N1, DTR=off, RTS=off, no flow control.", flush=True)
    print("No bytes will be transmitted. Close tuning software/serial monitors first.", flush=True)
    try:
        with open_passive_port(serial, args.port, args.baud) as port:
            print(f"Port open. Watch whether the Wi-Fi page stays reachable for {args.seconds:g}s.", flush=True)
            observe(port, args.seconds, lambda message: print(message, flush=True))
    except KeyboardInterrupt:
        print("\nStopped.")
    except (serial.SerialException, OSError) as exc:
        print(f"Serial probe failed: {exc}", file=sys.stderr)
        return 1
    print("Port closed. This probe cannot observe another application's DTR/RTS states.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
