#!/usr/bin/env python3
"""Hold the watch's serial port open and talk through it.

EVERY open of /dev/ttyACM0 resets the board (the CH343P's DTR/RTS dance
happens in the kernel driver before pyserial can lower the lines), so a
proof that must not reboot the watch keeps ONE process on the port:

    tools/monitor.py [-p /dev/ttyACM0] [-l LOG] [-f FIFO]     # the holder
    tools/monitor.py --send "status" [--send "wake"] ...      # into the FIFO

The holder appends every line the watch says to LOG (default
/tmp/watch.log, with the host's clock) and reads lines to send from
FIFO (default /tmp/watch.fifo). A dark watch loses the first bytes: the
sender writes a blank line, waits half a second, then the command
(0.4 s between commands). Stop the holder before `pio run -t upload`.

The pio venv has pyserial: ~/.local/share/pio-venv/bin/python tools/monitor.py
"""
import argparse
import os
import select
import sys
import time

import serial


def holder(port, log_path, fifo_path):
    if os.path.exists(fifo_path):
        os.remove(fifo_path)
    os.mkfifo(fifo_path)
    s = serial.Serial()
    s.port = port
    s.baudrate = 115200
    s.timeout = 0
    s.dtr = False
    s.rts = False
    s.open()
    log = open(log_path, "ab", buffering=0)
    fifo = os.open(fifo_path, os.O_RDONLY | os.O_NONBLOCK)
    buf = b""
    pending = b""
    print(f"holding {port}; log {log_path}; fifo {fifo_path}", flush=True)
    try:
        while True:
            r, _, _ = select.select([s.fileno(), fifo], [], [], 0.05)
            if s.fileno() in r:
                chunk = s.read(4096)
                if chunk:
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        stamp = time.strftime("%H:%M:%S").encode()
                        log.write(stamp + b" " + line.rstrip(b"\r") + b"\n")
            if fifo in r:
                try:
                    data = os.read(fifo, 4096)
                except BlockingIOError:
                    data = b""
                if not data:
                    # the writer closed: reopen for the next one
                    os.close(fifo)
                    fifo = os.open(fifo_path, os.O_RDONLY | os.O_NONBLOCK)
                else:
                    pending += data
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        s.write(line + b"\n")
                        s.flush()
                        log.write(time.strftime("%H:%M:%S").encode() + b" > " + line + b"\n")
                        time.sleep(0.4)
    finally:
        s.close()
        log.close()


def send(fifo_path, lines):
    with open(fifo_path, "w") as f:
        # a dark watch wakes on the UART's edges (threshold 3) and loses
        # the waking bytes: three blank lines are plenty of edges and
        # nothing to lose
        f.write("\n\n\n")
        f.flush()
        time.sleep(0.6)
        for ln in lines:
            f.write(ln + "\n")
            f.flush()
            time.sleep(0.45)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default="/dev/ttyACM0")
    ap.add_argument("-l", "--log", default="/tmp/watch.log")
    ap.add_argument("-f", "--fifo", default="/tmp/watch.fifo")
    ap.add_argument("--send", action="append", default=[])
    a = ap.parse_args()
    if a.send:
        send(a.fifo, a.send)
    else:
        holder(a.port, a.log, a.fifo)


if __name__ == "__main__":
    main()
