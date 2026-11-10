"""The project feature in the emulator (src/mm_kits.h, MM_PROJECT_ONLY), on a build from
`MM_KITS=project tools/build.sh`: one setup per project, in the project's data alone. The macro still moves its
destination on top of the kit; SAVE PROJECT AS writes the setup and the knobs into the project's header and nothing
into the kit records or the mod's card sectors; LOAD PROJECT brings both back; another pattern's kit changes nothing.

    python3 tests/emu/project_only.py --runner RUNNER --stock STOCK.syx --build out/macro-dt2.syx \\
        --map out/build/macro-dt2.map.json --out out/emu/project-only-dt2
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import scenario as S  # noqa: E402
import project as P  # noqa: E402


def poke_slot(r, k, j, dest, depth, tracks=0xffff):
    """Slot j of macro k as the setup screen would leave it; the tick then keeps it as the setup on."""
    L = r.syms['layout']
    a = r.syms['mm_state'] + L['m'] + L['macro'] * k + L['slot'] + L['slot_size'] * j
    r.cmd('poke %x %02x' % (a + L['dest'], dest))
    r.cmd('poke %x %04x' % (a + L['depth'], depth & 0xffff))
    r.cmd('poke %x %04x' % (a + L['tracks'], tracks))
    r.cmd('poke %x 01' % (r.syms['mm_state'] + L['dirty_config']))
    r.step(S.SETTLE)


def block(r):
    """The project's block in the mod's RAM (core's project data)."""
    a = r.syms['mm_scopes'] + r.syms['layout']['scope_proj']
    return bytes(r.peek8(a + k) for k in range(P.FMT['size']))


def scenario(r, out, dev):
    p = P.Project(r)
    fmt = P.PROJECT[dev]
    d0, d1, _ = fmt['dests']
    S.settle_boot(r)
    r.read_sounds()
    r.check(p.mode() == 0, 'the setup is the project\'s')
    r.check('mm_kitdata' not in r.syms and 'mm_ev_personalize' not in r.syms,
            'the build has no kit data and no MACRO SETUP row')

    # a macro on its destination, on top of the kit
    S.chord(r, S.PAGE, S.MOD)
    r.check(r.mode() == 1, 'PAGE + MOD shows the macro screen')
    poke_slot(r, 0, 0, d0, 64 * 256)
    S.spin_until(r, 1, -1, lambda: r.pos(0) <= -6)   # down: the firmware's kits have this one at its top
    pos = r.pos(0)
    r.step(S.SETTLE)
    S.check_frame(r, 0, 0, 'macro A turned')
    r.check(P.first_dest(block(r)) == d0 and P.position(block(r), 0) == pos,
            'the project\'s block holds the setup and knob A (%d)' % pos)

    # another pattern's kit: the same setup, the same knobs
    P.pattern(r, 1)
    r.check(r.slot(0, 0)[0] == d0 and r.pos(0) == pos, 'on A02 the setup and the knobs are the project\'s still')
    P.pattern(r, 0)
    r.tap(S.MOD)
    r.step(S.SETTLE)

    # SAVE PROJECT AS slot 001: the header carries the setup, the kit records and the card sectors nothing of the mod's
    before = P.card(r, out, 'card-0.bin')
    P.settings(r)
    P.menu(r, P.YES, P.UP, P.UP, P.DOWN, P.YES, P.YES, wait=2)
    r.shot('save-name')
    P.menu(r, P.YES, wait=8)
    r.shot('saved')
    after = P.card(r, out, 'card-1.bin')
    sec, rec, size = fmt['sector'], fmt['record'], fmt['size']
    r.check(any(after.get(k) != before.get(k) for k in range(sec, sec + 64)), 'the project was saved')
    h = P.read(after, sec, P.PROJ_AT, len(P.PROJ_HEAD) + size)
    r.check(h.startswith(P.PROJ_HEAD) and P.first_dest(h[len(P.PROJ_HEAD):]) == d0,
            'the header has the project\'s setup: %s' % h[:16].hex())
    r.check(P.position(h[len(P.PROJ_HEAD):], 0) == pos, 'and its knob A (%d)' % pos)
    kits = [P.read(after, sec, rec(i), P.KIT_HEAD + size) for i in range(3)]
    r.check(all(k == bytes(len(k)) for k in kits), 'the kit records hold nothing of the mod\'s')
    card = S.DEV['card']
    r.check(all(not any(after.get(card + i, b'')) for i in (0, 1)), 'the mod\'s old card sectors stay unwritten')
    P.menu(r, P.NO, P.NO, wait=2)

    # LOAD PROJECT 001: the setup and the knobs come back
    S.chord(r, S.PAGE, S.MOD)
    poke_slot(r, 0, 0, d1, 32 * 256)
    S.spin_until(r, 1, 1, lambda: r.pos(0) >= 6)
    r.check(r.slot(0, 0)[0] == d1 and r.pos(0) != pos, 'changed since the save: %d, knob A %d' % (r.slot(0, 0)[0], r.pos(0)))
    r.tap(S.MOD)
    r.step(S.SETTLE)
    P.settings(r)
    P.menu(r, P.YES, P.UP, P.UP, P.YES, wait=2)
    r.shot('load-list')
    P.menu(r, P.YES, wait=2)
    r.shot('load-confirm')
    P.menu(r, P.YES, wait=8)
    r.shot('loaded')
    P.menu(r, P.NO, P.NO, wait=2)
    r.check(r.slot(0, 0)[0] == d0 and r.slot(0, 0)[1] == 64 * 256, 'the load brings the project\'s setup back (%d)' % r.slot(0, 0)[0])
    r.check(r.pos(0) == pos, 'and its knob A (%d)' % r.pos(0))
    r.read_sounds()
    S.check_frame(r, 0, 0, 'after the load')
    r.check(p.conflicts() == 0, 'core met no bytes of another\'s in its areas')


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
    P.FMT = P.PROJECT[a.device]
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
