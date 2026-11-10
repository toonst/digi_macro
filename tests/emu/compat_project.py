# SPDX-License-Identifier: GPL-2.0-or-later
"""compat.py's round trips for a build with the project feature (`MM_KITS=project`: one setup per project, in the
header's gap alone), in the emulator:

1. the mod saves a project with a setup: slot 001. The kit records' tails hold nothing;
2. the stock OS, with that slot on its card, loads it and saves it again: slot 002. The two files may differ only in
   the header's gap and in what a load and a save change on the stock OS alone (the control, as in compat.py). In the
   gap the stock save has zeros or keeps the mod's setup;
3. the mod loads the stock OS's save: no conflict, and the setup is gone (zeros) or as it was; then its own save,
   still there, with the setup back;
4. a conflict: the gap of the mod's file holds something else (another tool's, say). The mod loads it: core counts a
   conflict and the mod has no setup from it. With a setup of its own it saves the project again: those bytes stay
   as they were, and the conflict is counted again.

    python3 tests/emu/compat_project.py --runner RUNNER --stock STOCK.syx --build out/macro-dt2.syx \\
        --map out/build/macro-dt2.map.json --out out/emu/compat-project-dt2
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import compat as C  # noqa: E402
import project as P  # noqa: E402
import scenario as S  # noqa: E402
from project_only import block, poke_slot  # noqa: E402


def project_dest(r):
    """Slot 1 of macro A's destination in the project's block, None without a setup."""
    return P.first_dest(block(r))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runner', required=True)
    ap.add_argument('--stock', required=True)
    ap.add_argument('--build', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--device', choices=sorted(S.DEVICES), default='dt2')
    ap.add_argument('--map', required=True)
    a = ap.parse_args()
    dev = a.device
    S.DEV = S.DEVICES[dev]
    P.FMT = P.PROJECT[dev]
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    syms = json.load(open(a.map))
    d0, d1, _ = P.PROJECT[dev]['dests']
    size = C.FILES[dev]
    gap = C.GAP
    tails = C.claimed(dev)[1:]
    n = failed = 0

    # 1. the mod saves slot 001 with a setup
    r = C.run(a, a.build, out, 'mod-save', syms=syms)
    try:
        S.settle_boot(r)
        poke_slot(r, 0, 0, d0, 64 * 256)
        C.save_as(r, 1, 'mod-saved')
        mod_card = P.card(r, out, 'card-mod.bin')
        r.check(project_dest(r) == d0, 'the mod saved with a setup (%s)' % project_dest(r))
    finally:
        r.close()
    n, failed = n + r.n, failed + r.failed
    one = P.read(mod_card, C.slot_sector(1), 0, size)

    # 2. the stock OS loads it and saves it again, as slot 002
    sec1 = C.slot_sector(1)
    card = C.card_file(mod_card, out / 'card-1.bin', (sec1, sec1 + size // 512 + 2), (0x56e00, 0x57000))
    r = C.run(a, a.stock, out, 'stock', card)
    try:
        S.settle_boot(r)
        C.save_as(r, 4, 'control-saved')
        C.load(r, 1, 'stock-loaded')
        C.save_as(r, 2, 'stock-saved')
        C.load(r, 3, 'control-loaded')
        C.save_as(r, 5, 'control-saved-again')
        stock_card = P.card(r, out, 'card-stock.bin')
    finally:
        r.close()
    r.n = r.failed = 0
    two = P.read(stock_card, C.slot_sector(2), 0, size)
    four, five = (P.read(stock_card, C.slot_sector(k), 0, size) for k in (4, 5))
    head = one[gap[0]:gap[0] + len(P.PROJ_HEAD)]
    r.check(head == P.PROJ_HEAD and P.first_dest(one[gap[0] + len(P.PROJ_HEAD):gap[1]]) == d0,
            'the mod\'s file has the setup in the header\'s gap: %s' % head.hex())
    r.check(all(one[lo:hi] == bytes(hi - lo) for lo, hi in tails), 'and nothing after the kit records\' end marks')
    own = bytearray(size)
    own[gap[0]:gap[1]] = b'\x01' * (gap[1] - gap[0])
    varies = [i for i in range(size) if four[i] != five[i] and not own[i]]
    r.check(four[:4] == five[:4] == b'\xbe\xef\xba\xce' and len(varies) <= 8,
            'the control: a load and a save on the stock OS alone change %d bytes: %s'
            % (len(varies), [hex(i) for i in varies[:12]]))
    r.check(two[:4] == b'\xbe\xef\xba\xce' and two[size - 4:] == b'\xba\xce\xf0\x0c',
            'the stock OS saved the project it loaded (slot 002)')
    mask = bytearray(own)
    for i in varies[:8]:
        mask[i] = 1
    outside = [i for i in range(size) if one[i] != two[i] and not mask[i]]
    r.check(not outside, 'the two saves differ only in the header\'s gap (%d other bytes differ, first at %s)'
            % (len(outside), hex(outside[0]) if outside else '-'))
    kept = two[gap[0]:gap[1]] == one[gap[0]:gap[1]]
    r.check(kept or two[gap[0]:gap[1]] == bytes(gap[1] - gap[0]),
            'in the gap the stock save has zeros, or the mod\'s setup as it was (%s)' % ('kept' if kept else 'zeros'))
    n, failed = n + r.n, failed + r.failed

    # 3. the mod loads the stock OS's save, then its own
    sec2 = C.slot_sector(2)
    card = C.card_file(stock_card, out / 'card-2.bin', (sec1, sec1 + size // 512 + 2), (sec2, sec2 + size // 512 + 2),
                       (0x56e00, 0x57000))
    r = C.run(a, a.build, out, 'mod-load', card, syms=syms)
    p = P.Project(r)
    try:
        S.settle_boot(r)
        poke_slot(r, 0, 0, d1, 32 * 256)            # a setup on that neither file has
        C.load(r, 2, 'mod-loaded')
        r.check(p.conflicts() == 0 and project_dest(r) in (None, d0),
                'the mod loads the stock save: no conflict (%d), the setup none or as it was (%s)'
                % (p.conflicts(), project_dest(r)))
        r.check(r.slot(0, 0)[0] != d1, 'and the setup that was on is gone (%d)' % r.slot(0, 0)[0])
        C.load(r, 1, 'mod-reloaded')
        r.check(project_dest(r) == d0 and r.slot(0, 0)[0] == d0, 'and its own save, still on the card: the setup is back')
    finally:
        r.close()
    n, failed = n + r.n, failed + r.failed

    # 4. someone else's bytes in the header's gap of slot 001
    foreign = bytes([0x42, 0x13] + [0x5a] * (gap[1] - gap[0] - 2))
    edited = dict(mod_card)
    for i, b in enumerate(foreign):
        k, o = sec1 + (gap[0] + i) // 512, (gap[0] + i) % 512
        edited[k] = edited[k][:o] + bytes([b]) + edited[k][o + 1:]
    card = C.card_file(edited, out / 'card-3.bin', (sec1, sec1 + size // 512 + 2), (0x56e00, 0x57000))
    r = C.run(a, a.build, out, 'mod-conflict', card, syms=syms)
    p = P.Project(r)
    try:
        S.settle_boot(r)
        C.save_as(r, 3, 'conflict-first')
        C.load(r, 1, 'conflict-loaded')             # the list: 001, 003
        conflicts = p.conflicts()
        r.check(conflicts >= 1 and project_dest(r) in (None, 0) and r.slot(0, 0)[0] == 0,
                'a load of someone else\'s bytes: a conflict (%d), and no setup from them (macro A: %d)'
                % (conflicts, r.slot(0, 0)[0]))
        poke_slot(r, 0, 0, d1, 32 * 256)
        C.save_as(r, 2, 'conflict-saved')
        after = p.conflicts()
        r.check(after > conflicts, 'the save found them in its way: more conflicts (%d, then %d)' % (conflicts, after))
        saved = P.read(P.card(r, out, 'card-conflict.bin'), C.slot_sector(2), 0, size)
        r.check(saved[gap[0]:gap[1]] == foreign, 'the mod\'s save leaves someone else\'s bytes as they were')
    finally:
        r.close()
    n, failed = n + r.n, failed + r.failed
    print('%d checks, %d failed' % (n, failed))
    return failed


if __name__ == '__main__':
    sys.exit(main())
