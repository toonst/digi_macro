# SPDX-License-Identifier: GPL-2.0-or-later
"""What macro mode adds to the frame interrupt, counted in instructions in the emulator (not a time measurement).

The frame interrupt is exception vector 191; its handler's address is read from the vector table (0x400002fc). For
each of FRAMES frames the run goes to the handler's first instruction, reads the interrupted PC from the exception
frame and runs to it: the difference of the instruction counts is the frame interrupt's length. With a mod build the
run also stops at mm_ev_frame_params' first instruction and at its return address: the hook's own length.

Three states of the mod: no macro set up; macro A with four destinations on all sixteen tracks, turned; all eight
macros with four destinations each on all sixteen tracks, turned (the most the setup holds), on the macro screen. The stock OS gives the
frame interrupt's length without the mod. A higher interrupt that comes in between is counted with the frame it
interrupts, so the report gives the median and the range.

    python3 tests/emu/frame_load.py --runner RUNNER --stock STOCK.syx --build out/macro-dt2.syx \\
        --map out/build/macro-dt2.map.json --out out/emu/frame-load-dt2
"""
import argparse
import json
import pathlib
import statistics
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import scenario as S  # noqa: E402

VECTOR = 0x400002fc       # exception vector 191's slot in the vector table
FRAMES = 200
MAX = 400_000_000


def until(r, pc):
    """Run to pc. -> the instruction count there."""
    state, count = r.cmd('run-until %x %d' % (pc, MAX)).split()
    if state != 'ok':
        raise SystemExit('PC %08x was not reached' % pc)
    return int(count)


def a7(r):
    return int(r.cmd('regs').split()[15], 16)


def frames(r, isr, hook=None):
    """[(the frame interrupt's instructions, the hook's)] for FRAMES frames."""
    out = []
    for _ in range(FRAMES):
        t0 = until(r, isr)
        back = r.peek32(a7(r) + 4)          # the exception frame: format and SR, then the interrupted PC
        own = 0
        if hook is not None:
            h0 = until(r, hook)
            own = until(r, r.peek32(a7(r))) - h0
        out.append((until(r, back) - t0, own))
    return out


def report(name, rows):
    isr, own = [a for a, _ in rows], [b for _, b in rows]
    line = dict(state=name, frames=len(rows), interrupt_median=int(statistics.median(isr)), interrupt_min=min(isr),
                interrupt_max=max(isr), hook_median=int(statistics.median(own)), hook_min=min(own), hook_max=max(own))
    print(json.dumps(line))
    return line


def setup(r, macros):
    """Macros 0 .. macros - 1 with four destinations each on all tracks at depth 64, as the setup screen would leave
    them, then each knob turned on the macro screen, which publishes the lists the interrupt reads."""
    L = r.syms['layout']
    if r.mode() != 1:
        S.chord(r, S.PAGE, S.MOD)
    for k in range(macros):
        m = r.syms['mm_state'] + L['m'] + L['macro'] * k
        for j in range(4):
            a = m + L['slot'] + L['slot_size'] * j
            r.cmd('poke %x %02x' % (a + L['dest'], 1 + 4 * k + j))
            r.cmd('poke %x %04x' % (a + L['depth'], 64 * 256))
            r.cmd('poke %x %04x' % (a + L['tracks'], 0xffff))
    r.cmd('poke %x 01' % (r.syms['mm_state'] + L['dirty_config']))
    r.step(S.SETTLE)
    for k in range(macros):
        S.spin_until(r, 1 + k, 1, lambda: r.pos(k) >= 20)
    r.step(S.SETTLE)
    return r.peek32(r.syms['mm_live']) != 0 and all(r.pos(k) >= 20 and r.slot(k, 3)[0] == 4 * k + 4
                                                    for k in range(macros))


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
    lines = []

    state = out / 'stock.state'
    state.unlink(missing_ok=True)
    r = S.Run([a.runner, a.stock, a.stock, str(state)], out)
    r.syms = None
    try:
        S.settle_boot(r)
        isr = r.peek32(VECTOR)
        lines.append(report('stock, stopped', frames(r, isr)))
        r.tap(S.PLAY)
        r.step(S.SETTLE)
        lines.append(report('stock, playing', frames(r, isr)))
    finally:
        r.close()

    state = out / 'build.state'
    state.unlink(missing_ok=True)
    r = S.Run([a.runner, a.stock, a.build, str(state)], out)
    r.syms = json.load(open(a.map))
    hook = r.syms['mm_ev_frame_params']
    try:
        S.settle_boot(r)
        if r.peek32(VECTOR) != isr:
            raise SystemExit('the build has another frame handler')
        lines.append(report('mod, no macro set up, stopped', frames(r, isr, hook)))
        for macros, name in ((1, 'mod, 1 macro x 4 destinations x 16 tracks'),
                             (8, 'mod, 8 macros x 4 destinations x 16 tracks')):
            if not setup(r, macros):
                raise SystemExit('the setup did not take')
            lines.append(report(name + ', stopped', frames(r, isr, hook)))
        r.tap(S.PLAY)
        r.step(S.SETTLE)
        lines.append(report('mod, 8 macros x 4 destinations x 16 tracks, playing', frames(r, isr, hook)))
    finally:
        r.close()
    (out / 'frame-load.json').write_text(json.dumps(lines, indent=1) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
