"""The kits feature in the emulator (docs/findings/s7-project-storage.md), on a build from tools/check.sh: the
PERSONALIZE row, a setup following the kit on (the pattern on's), the kits' and the project's setups kept apart across
scope changes, pattern copy and paste taking the kit's setup along, SAVE PROJECT AS writing them into the kit records
and the project's header, and LOAD PROJECT bringing them back.

    python3 tests/emu/project.py --runner RUNNER --stock STOCK.syx --build out/check/macro-dt2.syx \\
        --map out/check/macro-dt2.map.json --out out/emu-project-dt2 --device dt2
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import scenario as S  # noqa: E402

SETTINGS, YES, NO, UP, LEFT, DOWN, RIGHT, FUNC, RECORD, STOP, PTN = 8, 10, 12, 11, 13, 14, 15, 17, 19, 21, 23
# Per device: the saved project's first sector (slot 001: digikit finding 14), where kit n's mod data lies in the
# saved file (elekloader core's, after the kit record's end mark), and three destinations it has.
PROJECT = {
    'dt2': dict(sector=0x58000, record=lambda i: 0xae0200 + i * 0x5800 + 0x573e, dests=(36, 40, 50)),
    'dn2': dict(sector=0x58000, record=lambda i: 0xae0200 + i * 0x2a00 + 0x28af, dests=(67, 71, 80)),
}
SIZE = 191      # MM_SETUP_SIZE
# core's kit data: 'K', the layout's id, then the blocks (the mod's is the only one); project data in the header's
# gap at + 0x1c: 'ELKP', then the block's tag, its size and its bytes
KIT_HEAD, PROJ_AT, PROJ_HEAD = 3, 0x1c, b'ELKP' + b'MMPS' + (192).to_bytes(4, 'big')


def first_dest(rec):
    """Slot 1 of macro A's destination in a setup: the stream's first 7 bits; None for none (all zero)."""
    return rec[0] >> 1 if any(rec) else None


class Project:
    def __init__(self, r):
        self.r = r
        L = r.syms['layout']
        self.L = L
        self.scope = r.syms['mm_scopes']

    def mode(self):
        return self.r.peek8(self.scope + self.L['scope_mode'])

    def setup(self, i):
        a = self.scope + self.L['scope_kit'] + i * self.L['setup_size']
        return bytes(self.r.peek8(a + k) for k in range(8))   # a setup's first slot: none has these all zero

    def has_setup(self, i):
        return any(self.setup(i))

    def kit_dest(self, i):
        """Slot 1 of macro A's destination in kit i's setup, None without one."""
        return first_dest(self.setup(i))

    def conflicts(self):
        return self.r.peek32(self.r.syms['core_store_conflicts'])

    def clear(self, i):
        a = self.scope + self.L['scope_kit'] + i * self.L['setup_size']
        self.r.cmd('poke %x %s' % (a, '00' * self.L['setup_size']))

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

    # per project and back: the kits' setups stay as they are, and so does the project's
    scope(r, LEFT)
    r.check(r.slot(0, 0)[0] == 0, 'per project: the project\'s own, from boot (%d)' % r.slot(0, 0)[0])
    p.set_dest(0, 0, d2)
    r.check(p.kit_dest(0) == d0 and p.kit_dest(1) == d1, 'an edit per project leaves the kits\' setups: %s, %s'
            % (p.kit_dest(0), p.kit_dest(1)))
    scope(r, RIGHT)
    r.check(r.slot(0, 0)[0] == d0, 'per kit again: A01\'s kit\'s own (%d)' % r.slot(0, 0)[0])
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d1, 'and A02\'s (%d)' % r.slot(0, 0)[0])
    scope(r, LEFT)
    r.check(r.slot(0, 0)[0] == d2, 'per project again: the project\'s own (%d)' % r.slot(0, 0)[0])
    scope(r, RIGHT)

    # pattern copy and paste (FUNC + RECORD, FUNC + STOP): the pasted pattern's kit takes the setup along
    pattern(r, 0)
    r.down(FUNC)
    r.tap(RECORD)
    r.up(FUNC)
    pattern(r, 3)
    p.set_dest(0, 0, d1)                        # A04's kit: its own setup before the paste
    r.check(p.kit_dest(3) == d1, "A04's kit has its own setup before the paste")
    r.down(FUNC)
    r.tap(STOP)
    r.up(FUNC)
    r.step(S.SETTLE)
    r.step(S.SETTLE)
    r.shot('pasted')
    r.check(p.kit_dest(3) == d0 and r.slot(0, 0)[0] == d0,
            'the paste of A01 on A04 brings A01\'s kit\'s setup: kit %s, on %d' % (p.kit_dest(3), r.slot(0, 0)[0]))
    pattern(r, 0)
    scope(r, LEFT)

    # SAVE PROJECT AS slot 001, per project: every kit's record carries its own setup, the header the project's
    before = card(r, out, 'card-0.bin')
    settings(r)
    menu(r, YES, UP, UP, DOWN, YES, YES, wait=2)   # PROJECT keeps its cursor: from the top
    r.shot('save-name')
    menu(r, YES, wait=8)
    r.shot('saved')
    after = card(r, out, 'card-1.bin')
    sec, rec = PROJECT[dev]['sector'], PROJECT[dev]['record']
    r.check(any(after.get(k) != before.get(k) for k in range(sec, sec + 64)), 'the project was saved')
    a, b, c = (read(after, sec, rec(i), KIT_HEAD + SIZE) for i in range(3))
    h = read(after, sec, PROJ_AT, len(PROJ_HEAD) + SIZE)
    r.check(a[0] == 0x4b and first_dest(a[KIT_HEAD:]) == d0, 'the saved kit 1 has its own setup: %s' % a[:5].hex())
    r.check(b[:3] == a[:3] and first_dest(b[KIT_HEAD:]) == d1, 'the saved kit 2 has its own: %s' % b[:5].hex())
    r.check(c[:3] == a[:3] and c[KIT_HEAD:] == bytes(SIZE), 'kit 3, never on, has none')
    r.check(h.startswith(PROJ_HEAD) and first_dest(h[len(PROJ_HEAD):]) == d2,
            'the header has the project\'s: %s' % h[:16].hex())
    menu(r, NO, NO, wait=2)

    # LOAD PROJECT 001, per project: the kits get their setups back, and the project's comes on
    p.clear(0)
    p.clear(1)
    p.set_dest(0, 0, d1)
    settings(r)
    menu(r, YES, UP, UP, YES, wait=2)
    r.shot('load-list')
    menu(r, YES, wait=2)
    r.shot('load-confirm')
    menu(r, YES, wait=8)
    r.shot('loaded')
    menu(r, NO, NO, wait=2)
    r.check(p.kit_dest(0) == d0 and p.kit_dest(1) == d1, 'the load brought the kits\' setups back')
    r.check(r.slot(0, 0)[0] == d2, 'per project, the load brings the project\'s setup (%d)' % r.slot(0, 0)[0])
    pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d2, 'per project, another kit changes nothing (%d)' % r.slot(0, 0)[0])
    scope(r, RIGHT)
    r.check(r.slot(0, 0)[0] == d1, 'per kit, A02\'s kit\'s own (%d)' % r.slot(0, 0)[0])


def scope(r, key):
    """MACRO SETUP to per project (LEFT) or per kit (RIGHT), out of the menu again."""
    personalize(r)
    menu(r, key, NO, NO, NO, wait=2)


def settings(r):
    """SETTINGS open, at its first row. After a project load the firmware can let the first press go."""
    for _ in range(3):
        before = r.shot('settings-before') if r.syms is None else None
        menu(r, SETTINGS)
        if r.syms is None:          # the stock OS: no map to look with, so the screen must have changed
            if r.shot('settings-after') != before:
                break
        elif S.top_view(r) != S.DEV['view_top']:
            break
        r.step(S.SETTLE)
    menu(r, *[UP] * 10)


def personalize(r):
    settings(r)
    menu(r, *[DOWN] * 6, YES, *[DOWN] * 16)


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
