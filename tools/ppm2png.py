#!/usr/bin/env python3
"""PPM -> PNG (and an optional contact sheet), stdlib only."""
import struct
import sys
import zlib


def read_ppm(path):
    with open(path, "rb") as f:
        assert f.readline().strip() == b"P6"
        w, h = map(int, f.readline().split())
        f.readline()
        return w, h, f.read(w * h * 3)


def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    args = sys.argv[1:]
    cols = 4
    if args and args[0] == "--cols":          # a strip: --cols 6
        cols = int(args[1])
        args = args[2:]
    out = args[0]
    files = args[1:]
    imgs = [read_ppm(p) for p in files]
    w, h = imgs[0][0], imgs[0][1]
    cols = min(cols, len(imgs))
    rows = (len(imgs) + cols - 1) // cols
    sheet = bytearray(cols * w * rows * h * 3)
    for i, (iw, ih, rgb) in enumerate(imgs):
        ox, oy = (i % cols) * w, (i // cols) * h
        for y in range(ih):
            start = ((oy + y) * cols * w + ox) * 3
            sheet[start:start + iw * 3] = rgb[y * iw * 3:(y + 1) * iw * 3]
    write_png(out, cols * w, rows * h, bytes(sheet))


if __name__ == "__main__":
    main()
