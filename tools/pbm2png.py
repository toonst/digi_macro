# SPDX-License-Identifier: GPL-2.0-or-later
"""Turn the plain PBM screens of tests/preview.c into PNGs, scaled up, as an OLED: white on black.

    python3 tools/pbm2png.py OUT_DIR IN.pbm [IN.pbm ...]
"""
import os
import struct
import sys
import zlib

SCALE = 4


def read_pbm(path):
    with open(path) as f:
        tokens = f.read().split()
    assert tokens[0] == 'P1', path
    w, h = int(tokens[1]), int(tokens[2])
    bits = ''.join(tokens[3:])
    return w, h, [[bits[y * w + x] == '1' for x in range(w)] for y in range(h)]


def write_png(path, w, h, rows):
    raw = bytearray()
    for y in range(h * SCALE):
        raw.append(0)
        for x in range(w * SCALE):
            on = rows[y // SCALE][x // SCALE]
            raw += b'\xe8\xf4\xff' if on else b'\x08\x08\x10'
    def chunk(kind, data):
        c = struct.pack('>I', len(data)) + kind + data
        return c + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w * SCALE, h * SCALE, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    png += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def main(argv):
    out = argv[1]
    os.makedirs(out, exist_ok=True)
    for p in argv[2:]:
        w, h, rows = read_pbm(p)
        write_png(os.path.join(out, os.path.splitext(os.path.basename(p))[0] + '.png'), w, h, rows)


if __name__ == '__main__':
    main(sys.argv)
