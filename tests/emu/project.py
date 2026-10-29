"""Setups per pattern and per project in the emulator (docs/findings/s7-project-storage.md), on a build from
tools/check.sh: the PERSONALIZE row, a setup following the pattern on, SAVE PROJECT AS writing each pattern's setup
into the project's records, and LOAD PROJECT bringing them back.

    python3 tests/emu/project.py --runner RUNNER --stock STOCK.syx --build out/check/macro-dt2.syx \\
        --map out/check/macro-dt2.map.json --out out/emu-project-dt2 --device dt2
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import scenario as S  # noqa: E402

SETTINGS, YES, NO, UP, LEFT, DOWN, RIGHT, PTN = 8, 10, 12, 11, 13, 14, 15, 23
# Per device: the saved project's first sector (slot 001: digikit finding 14), and where a pattern's setup lies in
# the saved file: the pattern record's unused tail on the Digitakt II, the kit record's on the Digitone II.
PROJECT = {
    'dt2': dict(sector=0x58000, record=lambda i: 512 + i * 0x15c00 + 0x15acc, dests=(36, 40, 50)),
    'dn2': dict(sector=0x58000, record=lambda i: 0xae0200 + i * 0x2a00 + 0x28b0, dests=(67, 71, 80)),
}


class Project:
    def __init__(self, r):
        self.r = r
        L = r.syms['layout']
        self.L = L
        self.scope = r.syms['mm_scopes']

    def mode(self):
        return self.r.peek8(self.scope + self.L['scope_mode'])

    def has_setup(self, i):
        a = self.scope + self.L['scope_pat'] + i * self.L['setup_size']
        return self.r.peek8(a) == ord('M') and self.r.peek8(a + 1) == ord('S')

    def clear(self, i):
        a = self.scope + self.L['scope_pat'] + i * self.L['setup_size']
        self.r.cmd('poke %x %s' % (a, '00' * 4))

    def set_dest(self, k, j, dest):
        """Slot j of macro k to dest, as the setup screen would (the tick then keeps it as the setup on)."""
        L = self.L
        a = self.r.syms['mm_state'] + L['m'] + L['macro'] * k + L['slot'] + L['slot_size'] * j + L['dest']
        self.r.cmd('poke %x %02x' % (a, dest))
        self.r.cmd('poke %x 01' % (self.r.syms['mm_state'] + L['dirty_config']))
        self.r.step(S.SETTLE)


def pattern(r, n):
    """Pattern A0n + 1 on: PTN + trig n + 1."""
    r.down(PTN)
    r.tap(S.TRIG1 + n)
    r.up(PTN)
    r.step(S.SETTLE)
    r.step(S.SETTLE)


def menu(r, *keys, wait=1):
    for k in keys:
        r.tap(k)
        r.step(S.SETTLE)
    for _ in range(wait - 1):
        r.step(S.SETTLE)


def card(r, out, name):
    path = out / name
    r.cmd('card-save %s' % path)
    d = path.read_bytes()
    return {int.from_bytes(d[i:i + 8], 'big'): d[i + 8:i + 520] for i in range(0, len(d), 520)}


def read(sectors, base, off, n):
    out = b''
    while n > 0:
        s, o = base + off // 512, off % 512
        k = min(n, 512 - o)
        out += sectors.get(s, b'\0' * 512)[o:o + k]
        off, n = off + k, n - k
    return out


def scenario(r, out, dev):
    p = Project(r)
    S.settle_boot(r)
    r.check(p.mode() == 0, 'setups start per project')

    # SETTINGS > PERSONALIZE (the seventh row), then down to its last row: MACRO SETUP
    menu(r, SETTINGS, *[DOWN] * 6, YES, *[DOWN] * 16)
    r.shot('personalize')
    menu(r, YES)
    r.shot('personalize-pattern')
    r.check(p.mode() == 1, 'YES on MACRO SETUP: per pattern')
    menu(r, LEFT)
    r.check(p.mode() == 0, 'LEFT: per project')
    menu(r, RIGHT)
    r.check(p.mode() == 1, 'RIGHT: per pattern')
    menu(r, NO, NO, NO, wait=2)

    # each pattern its own setup; one it has not got yet starts as a copy of the one on
    d0, d1, d2 = PROJECT[dev]['dests']
    r.check(p.has_setup(0), 'pattern A01 has a setup')
    p.set_dest(0, 0, d0)
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d0 and p.has_setup(1), 'A02 starts as a copy of A01\'s setup')
    p.set_dest(0, 0, d1)
    pattern(r, 0)
    r.check(r.slot(0, 0)[0] == d0, 'back on A01: its own setup (%d)' % r.slot(0, 0)[0])
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d1, 'A02: its own (%d)' % r.slot(0, 0)[0])
    pattern(r, 0)

    # SAVE PROJECT AS slot 001: every pattern's record carries its setup
    before = card(r, out, 'card-0.bin')
    menu(r, SETTINGS, *[UP] * 10, YES, DOWN, YES, YES, wait=2)
    r.shot('save-name')
    menu(r, YES, wait=8)
    r.shot('saved')
    after = card(r, out, 'card-1.bin')
    sec, rec = PROJECT[dev]['sector'], PROJECT[dev]['record']
    r.check(any(after.get(k) != before.get(k) for k in range(sec, sec + 64)), 'the project was saved')
    a, b, c = (read(after, sec, rec(i), 205) for i in (0, 1, 2))
    r.check(a[:2] == b'MS' and a[3] == d0, 'the saved A01 has its setup: %s' % a[:4].hex())
    r.check(b[:2] == b'MS' and b[3] == d1, 'the saved A02 has its own: %s' % b[:4].hex())
    r.check(c == bytes(205), 'A03, never on, has none')
    menu(r, NO, NO, wait=2)

    # LOAD PROJECT 001 brings them back
    p.clear(0)
    p.clear(1)
    p.set_dest(0, 0, d2)
    menu(r, SETTINGS, *[UP] * 10, YES, UP, YES, wait=2)
    r.shot('load-list')
    menu(r, YES, wait=2)
    r.shot('load-confirm')
    menu(r, YES, wait=8)
    r.shot('loaded')
    menu(r, NO, NO, wait=2)
    r.check(p.has_setup(0) and p.has_setup(1), 'the load brought A01\'s and A02\'s setups back')
    r.check(r.slot(0, 0)[0] == d0, 'the pattern on after the load has its setup on (%d)' % r.slot(0, 0)[0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runner', required=True)
    ap.add_argument('--stock', required=True)
    ap.add_argument('--build', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--device', choices=sorted(S.DEVICES), default='dt2')
    ap.add_argument('--map', required=True)
    a = ap.parse_args()
    S.DEV = S.DEVICES[a.device]
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    state = out / 'build.state'
    state.unlink(missing_ok=True)
    r = S.Run([a.runner, a.stock, a.build, str(state)], out)
    r.syms = json.load(open(a.map))
    try:
        scenario(r, out, a.device)
    finally:
        r.close()
    print('%d checks, %d failed' % (r.n, r.failed))
    return r.failed


if __name__ == '__main__':
    sys.exit(main())
