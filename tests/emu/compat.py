"""Projects stay compatible with and without the mod (docs/DEVELOPING.md, "Storage and other mods"), in the emulator:

1. the mod (a build with the kits feature) saves a project with setups per kit and the project's: slot 001;
2. the stock OS, with that slot on its card, loads it and saves it again: slot 002. The mod's file and the stock
   OS's may differ only in the bytes elekloader's core keeps mods' data in (after each kit record's end mark, the
   header's gap), and in what
   a load and a save change on the stock OS alone (the control: its own project saved as slot 004, loaded, saved as
   slot 005; a word of the header). In those bytes the stock save has zeros or keeps the mod's setups;
3. the mod loads the stock OS's save: no conflict, and the setups are either gone (zeros) or as they were; then its
   own save, still there, with the setups back;
4. a conflict: kit 1's bytes in the mod's file hold something else (another tool's, say). The mod loads it: core
   counts a conflict and does not take them for a setup. Kit 1 then gets a setup of the mod's own, and the mod saves the
   project again (after a first save, so the firmware's save starts from the loaded file, as in step 2): those bytes
   stay as they were, the conflict is counted again, and kit 2's setup is saved as before.

    python3 tests/emu/compat.py --runner RUNNER --stock STOCK.syx --build out/check/macro-dt2.syx \\
        --map out/check/macro-dt2.map.json --out out/emu-compat-dt2 --device dt2
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import project as P  # noqa: E402
import scenario as S  # noqa: E402

FILES = {'dt2': 0xdd9604, 'dn2': 0xc4b004}   # a saved project's length: its end mark's place + 4 (s7)
KITS = {'dt2': (0xae0200, 0x5800, 128), 'dn2': (0xae0200, 0x2a00, 129)}   # the kit records in the file: start, stride, count
TAILS = {'dt2': (0x573e, 0x5800), 'dn2': (0x28af, 0x2a00)}   # a kit record's bytes after its end mark
GAP = (0x1c, 0x200)                                          # the header's


def slot_sector(n):
    """Project slot n's first card sector (digikit finding 14), n from 1."""
    return ((n - 1 + 0xb) & 0xff) << 15


def claimed(dev):
    """[(lo, hi)] of the file's bytes core keeps mods' data in: the header's gap, then each kit record's tail."""
    (start, stride, count), (lo, hi) = KITS[dev], TAILS[dev]
    return [GAP] + [(start + i * stride + lo, start + i * stride + hi) for i in range(count)]


def card_file(sectors, path, *ranges):
    """The sectors of the given card ranges, as the runner's card file."""
    keep = sorted(k for k in sectors if any(a <= k < b for a, b in ranges))
    path.write_bytes(b''.join(k.to_bytes(8, 'big') + sectors[k] for k in keep))
    return path


def run(a, build, out, name, card=None, syms=None):
    state = out / (name + '.state')
    state.unlink(missing_ok=True)
    r = S.Run([a.runner, a.stock, build, str(state)] + ([str(card)] if card else []), out)
    r.syms = syms
    return r


def save_as(r, slot, name):
    """SAVE PROJECT AS into slot (1 ..), the name as it is."""
    P.settings(r)
    P.menu(r, P.YES, P.UP, P.UP, P.DOWN, P.YES, *[P.UP] * 8, *[P.DOWN] * (slot - 1), P.YES, wait=2)
    r.shot(name + '-name')
    P.menu(r, P.YES, wait=8)
    r.shot(name)
    P.menu(r, *[P.NO] * 4, wait=2)          # back to the main screen, whatever dialogs came


def load(r, row, name):
    """LOAD PROJECT of the project on row (1 ..) of its list, which has the saved projects only, by slot."""
    P.settings(r)
    P.menu(r, P.YES, P.UP, P.UP, P.YES, *[P.UP] * 8, *[P.DOWN] * (row - 1), wait=2)  # the submenu keeps its cursor
    r.shot(name + '-list')
    P.menu(r, P.YES, wait=2)
    r.shot(name + '-confirm')
    P.menu(r, P.YES, wait=16)               # a confirmation (DT2), or the list again (DN2 loads without one)
    r.shot(name)
    P.menu(r, *[P.NO] * 4, wait=4)          # back to the main screen, whatever dialogs came


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
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    syms = json.load(open(a.map))
    d0, d1, d2 = P.PROJECT[dev]['dests']
    global FILE
    FILE = FILES[dev]
    n = failed = 0

    # 1. the mod saves slot 001: kit 1 and kit 2 with setups of their own, and the project's
    r = run(a, a.build, out, 'mod-save', syms=syms)
    p = P.Project(r)
    try:
        S.settle_boot(r)
        P.scope(r, P.RIGHT)
        p.set_dest(0, 0, d0)
        P.pattern(r, 1)
        p.set_dest(0, 0, d1)
        P.pattern(r, 0)
        P.scope(r, P.LEFT)
        p.set_dest(0, 0, d2)
        save_as(r, 1, 'mod-saved')
        mod_card = P.card(r, out, 'card-mod.bin')
        r.check(p.kit_dest(0) == d0 and p.kit_dest(1) == d1, 'the mod saved with setups on kits 1 and 2')
    finally:
        r.close()
    n, failed = n + r.n, failed + r.failed
    one = P.read(mod_card, slot_sector(1), 0, FILE)

    # 2. the stock OS loads it and saves it again, as slot 002
    sec1 = slot_sector(1)
    card = card_file(mod_card, out / 'card-1.bin', (sec1, sec1 + FILE // 512 + 2), (0x56e00, 0x57000))
    r = run(a, a.stock, out, 'stock', card)
    try:
        S.settle_boot(r)
        save_as(r, 4, 'control-saved')              # its own project (a save makes it the one loaded)
        load(r, 1, 'stock-loaded')                  # the list: 001, 004
        save_as(r, 2, 'stock-saved')
        load(r, 3, 'control-loaded')                # the list: 001, 002, 004; never the one loaded
        save_as(r, 5, 'control-saved-again')
        stock_card = P.card(r, out, 'card-stock.bin')
    finally:
        r.close()
    r.n = r.failed = 0                  # the checks below are this run's results
    two = P.read(stock_card, slot_sector(2), 0, FILE)
    four, five = (P.read(stock_card, slot_sector(k), 0, FILE) for k in (4, 5))
    ours = claimed(dev)
    own = bytearray(FILE)
    for lo, hi in ours:
        own[lo:hi] = b'\x01' * (hi - lo)
    varies = [i for i in range(FILE) if four[i] != five[i] and not own[i]]
    r.check(four[:4] == five[:4] == b'\xbe\xef\xba\xce' and len(varies) <= 8,
            'the control: a load and a save on the stock OS alone change %d bytes: %s'
            % (len(varies), [hex(i) for i in varies[:12]]))
    r.check(two[:4] == b'\xbe\xef\xba\xce' and two[FILE - 4:] == b'\xba\xce\xf0\x0c',
            'the stock OS saved the project it loaded (slot 002)')
    mask = bytearray(own)
    for i in varies[:8]:                # a control that went wrong excuses nothing
        mask[i] = 1
    outside = [i for i in range(FILE) if one[i] != two[i] and not mask[i]]
    r.check(not outside, 'the two saves differ only in the bytes the mod claims (%d other bytes differ, first at %s)'
            % (len(outside), hex(outside[0]) if outside else '-'))
    kept = all(two[lo:hi] in (bytes(hi - lo), one[lo:hi]) for lo, hi in ours)
    r.check(kept, 'in the mod\'s bytes the stock save has zeros, or the mod\'s setups as they were (%s)'
            % ('kept' if two[ours[0][0]:ours[0][1]] == one[ours[0][0]:ours[0][1]] else 'zeros'))
    n, failed = n + r.n, failed + r.failed

    # 3. the mod loads the stock OS's save: no setups, no conflict
    sec2 = slot_sector(2)
    card = card_file(stock_card, out / 'card-2.bin', (sec1, sec1 + FILE // 512 + 2), (sec2, sec2 + FILE // 512 + 2),
                     (0x56e00, 0x57000))
    r = run(a, a.build, out, 'mod-load', card, syms=syms)
    p = P.Project(r)
    try:
        S.settle_boot(r)
        load(r, 2, 'mod-loaded')                    # the list: 001, 002
        conflicts = p.conflicts()
        r.check(conflicts == 0 and (p.kit_dest(0), p.kit_dest(1)) in ((None, None), (d0, d1)),
                'the mod loads the stock save: no conflict (%d), setups none or as they were (%s, %s)'
                % (conflicts, p.kit_dest(0), p.kit_dest(1)))
        load(r, 1, 'mod-reloaded')
        r.check(p.kit_dest(0) == d0 and p.kit_dest(1) == d1 and r.slot(0, 0)[0] == d2,
                'and its own save, still on the card: the setups are back')
    finally:
        r.close()
    n, failed = n + r.n, failed + r.failed

    # 4. someone else's bytes in kit 1's area of slot 001
    kit1 = claimed(dev)[1]
    foreign = bytes([0x42, 0x13] + [0x5a] * (kit1[1] - kit1[0] - 2))
    edited = dict(mod_card)
    for i, b in enumerate(foreign):
        k, o = sec1 + (kit1[0] + i) // 512, (kit1[0] + i) % 512
        edited[k] = edited[k][:o] + bytes([b]) + edited[k][o + 1:]
    card = card_file(edited, out / 'card-3.bin', (sec1, sec1 + FILE // 512 + 2), (0x56e00, 0x57000))
    r = run(a, a.build, out, 'mod-conflict', card, syms=syms)
    p = P.Project(r)
    try:
        S.settle_boot(r)
        save_as(r, 3, 'conflict-first')
        load(r, 1, 'conflict-loaded')               # the list: 001, 003
        conflicts = p.conflicts()
        r.check(conflicts >= 1 and p.kit_dest(0) is None and p.kit_dest(1) == d1,
                'a load of someone else\'s bytes: a conflict (%d), kit 1 has no setup, kit 2 its own' % conflicts)
        P.scope(r, P.RIGHT)                         # per kit: kit 1, on, takes a copy of the setup on
        r.check(p.kit_dest(0) is not None, 'kit 1 has a setup of the mod\'s now')
        save_as(r, 2, 'conflict-saved')
        after = p.conflicts()  # each time the firmware passes the record: twice a save
        r.check(after > conflicts, 'the save found them in its way: more conflicts (%d, then %d)' % (conflicts, after))
        conflict_card = P.card(r, out, 'card-conflict.bin')
    finally:
        r.close()
    again = P.read(conflict_card, sec2, 0, FILE)
    r.check(again[kit1[0]:kit1[1]] == foreign, 'the mod\'s save leaves someone else\'s bytes as they were')
    k2 = (kit1[0] + KITS[dev][1], kit1[1] + KITS[dev][1])
    r.check(again[k2[0]:k2[1]] == one[k2[0]:k2[1]], 'and saves kit 2\'s setup as before')
    n, failed = n + r.n, failed + r.failed
    print('%d checks, %d failed' % (n, failed))
    return failed


if __name__ == '__main__':
    sys.exit(main())
