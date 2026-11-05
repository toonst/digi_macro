# SPDX-License-Identifier: GPL-2.0-or-later
"""The README's macro screen, captured in digikit's emulator with the stock header: pattern A01 renamed DEKTRO with
the stock naming screen (SETTINGS > PATTERN > RENAME), the macros of tests/preview.c's turned screen put in the mode's
state, then macro mode entered and macro B turned, so its value shows.

    python3 tests/emu/readme_shot.py --runner RUNNER --stock STOCK.syx --build out/macro-dt2.syx \\
        --map out/build/macro-dt2.map.json --out out/emu/readme

It writes docs/img/macro-screen.png (tools/pbm2png.py's look); tools/check.sh --previews leaves it alone.
"""
import argparse
import json
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import scenario as S  # noqa: E402

UP, DOWN, LEFT, RIGHT, YES, NO, FUNC, SETTINGS = 11, 14, 13, 15, 10, 12, 17, 8
CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
ROOT = pathlib.Path(__file__).resolve().parents[2]


def taps(r, key, n):
    for _ in range(n):
        r.tap(key)


def rename(r, name, old='UNTITLED'):
    """The pattern on renamed with the naming screen: UP and DOWN step a character, RIGHT moves on, FUNC + NO deletes
    the character before the cursor, YES saves."""
    r.tap(SETTINGS)
    taps(r, DOWN, 2)
    r.tap(YES)
    r.tap(YES)
    r.step(S.SETTLE)
    for have, want in zip(old, name):
        d = CHARS.index(want) - CHARS.index(have)
        taps(r, UP if d > 0 else DOWN, abs(d))
        r.tap(RIGHT)
    taps(r, RIGHT, len(old) - len(name))
    for _ in range(len(old) - len(name)):
        r.down(FUNC)
        r.tap(NO)
        r.up(FUNC)
    r.tap(YES)
    r.step(S.SETTLE)
    taps(r, NO, 3)
    r.step(S.SETTLE)


def put_macros(r):
    """tests/preview.c's turned screen: A, B, C, D (named BASS) and F with a parameter, E, G and H none."""
    L, st = r.syms['layout'], r.syms['mm_state']

    def macro(k):
        return st + L['m'] + L['macro'] * k

    for k, dest, depth, tracks in ((0, 37, 20, 0xffff), (1, 25, -8, 0x0003), (2, 36, 40, 0x0005),
                                   (3, 55, 30, 0x0001), (5, 36, -40, 0xffff)):
        a = macro(k) + L['slot']
        r.cmd('poke %x %02x' % (a + L['dest'], dest))
        r.cmd('poke %x %04x' % (a + L['depth'], depth * 512 & 0xffff))
        r.cmd('poke %x %04x' % (a + L['tracks'], tracks))
    for k, pos in ((0, 40), (2, 63), (5, -64)):
        r.cmd('poke %x %02x' % (macro(k) + L['pos'], pos & 0xff))
    r.cmd('poke %x %s' % (macro(3) + L['name'], b'BASS\0\0'.hex()))
    r.step(S.SETTLE)


def main():
    ap = argparse.ArgumentParser()
    for name in ('runner', 'stock', 'build', 'map', 'out'):
        ap.add_argument('--' + name, required=True)
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    state = out / 'build.state'
    state.unlink(missing_ok=True)
    r = S.Run([a.runner, a.stock, a.build, str(state)], out)
    r.syms = json.load(open(a.map))
    try:
        S.settle_boot(r)
        rename(r, 'DEKTRO')
        put_macros(r)
        S.chord(r, S.PAGE, S.MOD)
        r.step(S.SETTLE)
        S.spin_until(r, 2, -1, lambda: r.pos(1) <= -25)
        r.shot('macro-screen')
    finally:
        r.close()
    frame = (out / 'macro-screen.oled').read_bytes()
    pbm = out / 'macro-screen.pbm'
    rows = [''.join('1' if frame[(7 - y // 8) + 8 * x] >> (y % 8) & 1 else '0' for x in range(128)) for y in range(64)]
    pbm.write_text('P1\n128 64\n' + '\n'.join(rows) + '\n')
    subprocess.run([sys.executable, str(ROOT / 'tools' / 'pbm2png.py'), str(ROOT / 'docs' / 'img'), str(pbm)], check=True)
    print('wrote docs/img/macro-screen.png')


if __name__ == '__main__':
    sys.exit(main())
