"""
Ramzor Bridge - Python listener

Reads JSON lines from the ESP32 bridge over USB serial and prints them.
"""

import json
import sys
import time

import serial

PORT = "COM5"   # <-- Fallback only, used if auto-detect below finds nothing.
BAUD = 115200

# Description keywords for common ESP32 USB-serial chips (Windows shows these
# in the port description; Linux shows them in the device path).
USB_SERIAL_HINTS = ["CP210", "CH340", "CH9102", "USB-SERIAL", "USB Serial", "Silicon Labs", "USB"]


def list_ports():
    from serial.tools import list_ports as lp
    ports = list(lp.comports())
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for p in ports:
        print(f"  {p.device} - {p.description}")


def find_port():
    """Try to auto-detect a USB-serial device (ESP32) on Windows or Linux.
    Matches on device path (Linux: ttyUSB*/ttyACM*) or description (Windows:
    CP210x/CH340/etc. chip names). Falls back to PORT if nothing matches."""
    from serial.tools import list_ports as lp
    ports = list(lp.comports())

    # Linux-style device paths
    for p in ports:
        if "ttyUSB" in p.device or "ttyACM" in p.device:
            return p.device

    # Windows-style: match on description/manufacturer keywords
    for p in ports:
        text = f"{p.description} {p.manufacturer or ''}"
        if any(hint.lower() in text.lower() for hint in USB_SERIAL_HINTS):
            return p.device

    return PORT


def handle_message(msg: dict):
    """Called once per parsed JSON message. Extend this as needed."""
    device = msg.get("device", "unknown")
    mtype = msg.get("type", "unknown")

    if mtype == "traffic_light":
        state = msg.get("state", "?")
        reason = msg.get("reason", "?")
        print(f"[{device}] traffic light -> {state}  (reason: {reason})")

    elif mtype == "train_control":
        direction = msg.get("direction")
        speed = msg.get("speed")
        location = msg.get("location")
        print(f"[{device}] train control -> direction={direction} speed={speed} location={location}")

    elif mtype == "unknown":
        print(f"[{device}] unrecognized message type: {msg}")

    else:
        # boot/ready/heartbeat/error events from the bridge itself
        print(f"(bridge) {msg}")


def main():
    if "--list" in sys.argv:
        list_ports()
        return

    port = find_port()
    print(f"Connecting to {port} at {BAUD} baud...")
    try:
        ser = serial.Serial(port, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"Could not open {port}: {e}")
        print("Run with --list to see available ports.")
        return

    time.sleep(2)
    print("Connected. Listening for messages (Ctrl+C to stop)...\n")

    while True:
        try:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            try:
                msg = json.loads(line)
                handle_message(msg)
            except json.JSONDecodeError:
                print(f"(raw) {line}")

        except KeyboardInterrupt:
            print("\nStopping.")
            break
        except Exception as e:
            print(f"Error: {e}")


if __name__ == "__main__":
    main()
