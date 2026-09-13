#!/usr/bin/env python3
"""
capture_frame.py

Triggers the ESP-EYE bring-up sketch to dump one raw camera frame
over serial, and saves it as a viewable .pgm image (grayscale, no
compression — any image viewer or IrfanView/GIMP can open it).

Usage:
    python capture_frame.py COM15
    python capture_frame.py COM15 -o frame1.pgm

Requires: pip install pyserial
Close the PlatformIO Serial Monitor before running this — only one
program can hold the COM port at a time.
"""

import argparse
import time
import serial


def capture_frame(port: str, baud: int, output: str) -> None:
    with serial.Serial(port, baud, timeout=5) as ser:
        # Opening the port resets the ESP32 (DTR/RTS toggling) — give it
        # time to finish booting and reach loop() before sending anything,
        # or the command byte arrives before Serial.begin() and is lost.
        time.sleep(2)
        ser.reset_input_buffer()
        ser.write(b"c")

        # Read lines until we find the "FRAME w h len" header,
        # skipping any interleaved debug/status lines.
        header = None
        for _ in range(20):
            line = ser.readline().decode(errors="replace").strip()
            if line.startswith("FRAME "):
                header = line
                break
            if line:
                print(f"(skipped) {line}")

        if header is None:
            raise RuntimeError("Did not receive a FRAME header — check the board is running and 'c' was received")

        _, w, h, length = header.split()
        w, h, length = int(w), int(h), int(length)
        print(f"Receiving {w}x{h} frame, {length} bytes...")

        data = ser.read(length)
        if len(data) != length:
            raise RuntimeError(f"Expected {length} bytes, got {len(data)} — try again")

        with open(output, "wb") as f:
            f.write(f"P5\n{w} {h}\n255\n".encode())
            f.write(data)

        print(f"Saved {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Capture one frame from the ESP-EYE bring-up sketch")
    parser.add_argument("port", help="Serial port, e.g. COM15")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument("-o", "--output", default="frame.pgm")
    args = parser.parse_args()

    capture_frame(args.port, args.baud, args.output)
