#!/usr/bin/env python3
"""
Interactive serial terminal for the custom Arduino Nano STK500-lite firmware.

Protocol:
  Enter serial mode: 90 00 01 C2 00 20
  Exit serial mode:  90 00 00 00 00 20
  Send target data:  91 LEN_H LEN_L PAYLOAD 20

Receive modes:
  line    Display printable ASCII lines (CRLF, LF, or CR).off
  binary  Display received chunks as hex and printable text.

Install:
  python -m pip install pyserial
"""

import argparse
import threading
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Missing dependency: pyserial")
    print("Install it with: python -m pip install pyserial")
    raise SystemExit(1)


CMND_SERIAL_MODE = 0x90
CMND_SEND_SERIAL = 0x91
EOP = 0x20


def list_serial_ports():
    ports = list(list_ports.comports())

    if not ports:
        print("No serial ports found.")
        return []

    print("\nAvailable serial ports:")
    for i, port in enumerate(ports, 1):
        print(f"  {i}. {port.device} — {port.description}")

    return ports


def receive_worker(get_serial, stop_event, display_lock, mode_state):
    line_buffer = bytearray()
    pending_cr = False
    active_mode = None

    def display_line():
        stamp = time.strftime("%H:%M:%S")
        text = line_buffer.decode("ascii", errors="replace")
        print(f"\n[{stamp}] RX: {text}")
        print("> ", end="", flush=True)

    while not stop_event.is_set():
        with display_lock:
            mode = mode_state["mode"]

        if mode != active_mode:
            line_buffer.clear()
            pending_cr = False
            active_mode = mode

        ser = get_serial()
        if ser is None:
            line_buffer.clear()
            pending_cr = False
            time.sleep(0.1)
            continue

        try:
            data = ser.read(ser.in_waiting or 1)

            if not data:
                continue

            if mode == "binary":
                stamp = time.strftime("%H:%M:%S")
                printable = "".join(
                    chr(b) if 32 <= b <= 126 else "."
                    for b in data
                )

                with display_lock:
                    print(f"\n[{stamp}] RX {len(data)} byte(s)")
                    print("  HEX :", data.hex(" "))
                    print("  TEXT:", printable)
                    print("> ", end="", flush=True)
                continue

            for byte in data:
                if pending_cr:
                    with display_lock:
                        display_line()
                    line_buffer.clear()
                    pending_cr = False

                    # Treat CRLF as one line ending.off
                    if byte == 0x0A:
                        continue

                if byte == 0x0D:
                    pending_cr = True
                elif byte == 0x0A:
                    with display_lock:
                        display_line()
                    line_buffer.clear()
                elif byte == 0x09 or 0x20 <= byte <= 0x7E:
                    # Keep printable ASCII and tabs only.
                    line_buffer.append(byte)

        except (serial.SerialException, OSError) as exc:
            with display_lock:
                print(f"\nSerial receive error: {exc}")
            stop_event.set()
            break

    if line_buffer and mode_state["mode"] == "line":
        with display_lock:
            display_line()


def send_mode(ser, enabled, baud=115200):
    baud_value = baud if enabled else 0

    if enabled and not 1 <= baud_value <= 115200:
        print("Target baud must be between 1 and 115200.")
        return

    packet = (
        bytes((CMND_SERIAL_MODE,))
        + baud_value.to_bytes(4, "big")
        + bytes((EOP,))
    )

    ser.write(packet)
    ser.flush()

    mode = f"ON at {baud_value} baud" if enabled else "OFF"
    print(f"TX mode {mode}: {packet.hex(' ')}")


def send_target_data(ser, payload):
    if len(payload) > 128:
        print("Payload too long: maximum is 128 bytes.")
        return

    packet = (
        bytes((CMND_SEND_SERIAL,))
        + len(payload).to_bytes(2, "big")
        + payload
        + bytes((EOP,))
    )

    ser.write(packet)
    ser.flush()

    print(
        f"TX target payload ({len(payload)} byte(s)): "
        f"{payload.hex(' ')}"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Arduino custom serial terminal"
    )
    parser.add_argument(
        "--port",
        help="Serial port, e.g. COM5 or /dev/ttyUSB0"
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="USB serial baud rate (default: 115200)"
    )
    parser.add_argument(
        "--mode",
        choices=("line", "binary"),
        default="line",
        help="Initial receive mode (default: line)"
    )
    args = parser.parse_args()

    port_name = args.port

    if not port_name:
        ports = list_serial_ports()

        if ports:
            choice = input(
                "\nChoose port number or type port name: "
            ).strip()

            if choice.isdigit() and 1 <= int(choice) <= len(ports):
                port_name = ports[int(choice) - 1].device
            else:
                port_name = choice
        else:
            port_name = input("Enter serial port: ").strip()

    if not port_name:
        print("No port selected.")
        return

    if not 1 <= args.baud <= 115200:
        print("USB baud must be between 1 and 115200.")
        return

    # The port is not opened until the "on" command.
    ser = None
    serial_lock = threading.RLock()
    display_lock = threading.Lock()
    stop_event = threading.Event()
    mode_state = {"mode": args.mode}

    def get_serial():
        with serial_lock:
            if ser is not None and ser.is_open:
                return ser
            return None

    def connect(baud):
        nonlocal ser

        if not 1 <= baud <= 115200:
            print("Target baud must be between 1 and 115200.")
            return

        with serial_lock:
            if ser is not None and ser.is_open:
                print("Already connected. Use off before reconnecting.")
                return

            try:
                ser = serial.Serial(
                    port_name,
                    args.baud,
                    timeout=0.1,
                    write_timeout=2
                )

                # Some Arduino Nano USB adapters reset when opened.
                time.sleep(2.0)
                ser.reset_input_buffer()

                print(
                    f"Connected to {port_name} "
                    f"at {args.baud} baud."
                )
                send_mode(ser, True, baud)

            except (serial.SerialException, OSError) as exc:
                print(f"Could not connect to {port_name}: {exc}")

                if ser is not None:
                    try:
                        ser.close()
                    except (serial.SerialException, OSError):
                        pass

                ser = None

    def disconnect():
        nonlocal ser

        with serial_lock:
            if ser is None or not ser.is_open:
                print("Already disconnected.")
                ser = None
                return

            try:
                send_mode(ser, False)
            except (serial.SerialException, OSError) as exc:
                print(f"Serial shutdown warning: {exc}")
            finally:
                try:
                    ser.close()
                except (serial.SerialException, OSError) as exc:
                    print(f"Serial close warning: {exc}")

                ser = None
                print("Disconnected.")

    receiver = threading.Thread(
        target=receive_worker,
        args=(get_serial, stop_event, display_lock, mode_state),
        daemon=True
    )
    receiver.start()

    print(f"\nSelected port: {port_name} (not connected)")
    print(f"Receive mode: {mode_state['mode']}")
    print("\nCommands:")
    print("  on [BAUD]          Connect and enable target serial")
    print("  off                Disable target serial and disconnect")
    print("  send TEXT          Send UTF-8 text to the target")
    print("  hex HEXBYTES       Send bytes, e.g. hex 01 ff 41")
    print("  mode line|binary   Change receive display mode")
    print("  ports              List serial ports")
    print("  help               Show commands")
    print("  quit               Exit and disconnect")

    try:
        while True:
            try:
                line = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not line:
                continue

            parts = line.split(maxsplit=1)
            command = parts[0].lower()
            argument = parts[1] if len(parts) > 1 else ""

            if command == "quit":
                break

            elif command == "on":
                try:
                    baud = int(argument) if argument else 115200
                    connect(baud)
                except ValueError:
                    print("Usage: on [BAUD], e.g. on 115200")

            elif command == "off":
                disconnect()

            elif command == "ports":
                list_serial_ports()

            elif command == "mode":
                new_mode = argument.strip().lower()

                if new_mode not in ("line", "binary"):
                    print("Usage: mode line|binary")
                else:
                    with display_lock:
                        mode_state["mode"] = new_mode
                    print(f"Receive mode: {new_mode}")

            elif command == "help":
                print(
                    "on [BAUD] | off | send TEXT | hex HEXBYTES | "
                    "mode line|binary | ports | help | quit"
                )

            elif command in ("send", "hex") and argument:
                with serial_lock:
                    if ser is None or not ser.is_open:
                        print("Not connected. Enter 'on' first.")
                        continue

                    try:
                        if command == "send":
                            payload = argument.encode("utf-8")
                        else:
                            payload = bytes.fromhex(argument)

                        send_target_data(ser, payload)

                    except ValueError as exc:
                        print(f"Invalid hex: {exc}")
                    except (serial.SerialException, OSError) as exc:
                        print(f"Serial write error: {exc}")

            else:
                print("Unknown command. Type help.")

    finally:
        stop_event.set()
        disconnect()
        receiver.join(timeout=0.5)


if __name__ == "__main__":
    main()