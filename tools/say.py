#!/usr/bin/env python3
"""Talk to the watch over USB, one shot.

    tools/say.py [-p /dev/ttyACM0] [-t SECONDS] [--] [line ...]

Opens the port (DTR and RTS low -- but the open itself still RESETS
the board on this CH343P: the kernel driver toggles the lines before
pyserial can; use tools/monitor.py to hold the port across commands),
sends blank lines first and waits (a dark watch loses the first bytes:
the blank lines wake it), then each line in turn 0.4 s apart, and
prints everything the watch says for -t seconds (default 3). With no
lines it just listens.

The pio venv has pyserial: ~/.local/share/pio-venv/bin/python tools/say.py status
"""
import argparse
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    ap.add_argument("-t", "--listen", type=float, default=3.0)
    ap.add_argument("-g", "--gap", type=float, default=0.4)
    ap.add_argument("lines", nargs="*")
    a = ap.parse_args()
    s = serial.Serial()
    s.port = a.port
    s.baudrate = 115200
    s.timeout = 0.05
    s.dtr = False
    s.rts = False
    s.open()
    try:
        if a.lines:
            s.write(b"\n\n\n")           # a dark watch wakes on the UART's edges and loses them
            time.sleep(0.6)
            s.reset_input_buffer()
            for ln in a.lines:
                s.write(ln.encode() + b"\n")
                s.flush()
                time.sleep(a.gap)
        end = time.monotonic() + a.listen
        buf = b""
        while time.monotonic() < end:
            chunk = s.read(4096)
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    sys.stdout.write(line.decode("utf-8", "replace").rstrip("\r") + "\n")
                    sys.stdout.flush()
        if buf:
            sys.stdout.write(buf.decode("utf-8", "replace") + "\n")
    finally:
        s.close()


if __name__ == "__main__":
    main()
