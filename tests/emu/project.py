"""Setups per kit and per project in the emulator (docs/findings/s7-project-storage.md), on a build from
tools/check.sh: the PERSONALIZE row, a setup following the kit on (the pattern on's), SAVE PROJECT AS writing each
kit's setup into its record, and LOAD PROJECT bringing them back.

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
# Per device: the saved project's first sector (slot 001: digikit finding 14), where kit n's setup lies in the saved
# file (after the kit record's end mark), and three destinations it has.
PROJECT = {
    'dt2': dict(sector=0x58000, record=lambda i: 0xae0200 + i * 0x5800 + 0x573e, dests=(36, 40, 50)),
    'dn2': dict(sector=0x58000, record=lambda i: 0xae0200 + i * 0x2a00 + 0x28af, dests=(67, 71, 80)),
}
MARK, SIZE = 0xd1, 194


def first_dest(rec):
    """Slot 1 of macro A's destination in a saved setup: the stream's first 7 bits."""
    return rec[1] >> 1


class Project:
    def __init__(self, r):
        self.r = r
        L = r.syms['layout']
        self.L = L
        self.scope = r.syms['mm_scopes']

    def mode(self):
        return self.r.peek8(self.scope + self.L['scope_mode'])

    def has_setup(self, i):
        return self.r.peek8(self.scope + self.L['scope_kit'] + i * self.L['setup_size']) == MARK

    def clear(self, i):
        a = self.scope + self.L['scope_kit'] + i * self.L['setup_size']
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
    personalize(r)
    r.shot('personalize')
    menu(r, YES)
    r.shot('personalize-kit')
    r.check(p.mode() == 1, 'YES on MACRO SETUP: per kit')
    menu(r, LEFT)
    r.check(p.mode() == 0, 'LEFT: per project')
    menu(r, RIGHT)
    r.check(p.mode() == 1, 'RIGHT: per kit')
    menu(r, NO, NO, NO, wait=2)

    # each kit (the pattern's) its own setup; one it has not got yet starts as a copy of the one on
    d0, d1, d2 = PROJECT[dev]['dests']
    r.check(p.has_setup(0), "A01's kit has a setup")
    p.set_dest(0, 0, d0)
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d0 and p.has_setup(1), "A02's kit starts as a copy of A01's setup")
    p.set_dest(0, 0, d1)
    pattern(r, 0)
    r.check(r.slot(0, 0)[0] == d0, "back on A01: its kit's setup (%d)" % r.slot(0, 0)[0])
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d1, "A02: its kit's own (%d)" % r.slot(0, 0)[0])
    pattern(r, 0)

    # SAVE PROJECT AS slot 001: every kit's record carries its setup
    before = card(r, out, 'card-0.bin')
    menu(r, SETTINGS, *[UP] * 10, YES, DOWN, YES, YES, wait=2)
    r.shot('save-name')
    menu(r, YES, wait=8)
    r.shot('saved')
    after = card(r, out, 'card-1.bin')
    sec, rec = PROJECT[dev]['sector'], PROJECT[dev]['record']
    r.check(any(after.get(k) != before.get(k) for k in range(sec, sec + 64)), 'the project was saved')
    a, b, c = (read(after, sec, rec(i), SIZE) for i in (0, 1, 2))
    r.check(a[0] == MARK and first_dest(a) == d0, 'the saved kit 1 has its setup: %s' % a[:3].hex())
    r.check(b[0] == MARK and first_dest(b) == d1, 'the saved kit 2 has its own: %s' % b[:3].hex())
    r.check(c == bytes(SIZE), 'kit 3, never on, has none')
    menu(r, NO, NO, wait=2)

    # per project now, LOAD PROJECT 001: the kits get their setups back, and the project takes the kit on's
    personalize(r)
    menu(r, LEFT, NO, NO, NO, wait=2)
    r.check(p.mode() == 0, 'per project again')
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
    r.check(p.has_setup(0) and p.has_setup(1), "the load brought kit 1's and kit 2's setups back")
    r.check(r.slot(0, 0)[0] == d0, "per project, the project load brings the kit on's setup (%d)" % r.slot(0, 0)[0])
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d0, 'per project, another kit changes nothing (%d)' % r.slot(0, 0)[0])


def personalize(r):
    menu(r, SETTINGS, *[UP] * 10, *[DOWN] * 6, YES, *[DOWN] * 16)


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
