# SPDX-License-Identifier: GPL-2.0-or-later
"""Drive a macro-mode build in digikit's emulator and check the screen, the LEDs and the frame values (PLAN.md 8).

    python3 tests/emu/scenario.py --runner ZERO_PATCHED --stock STOCK.syx --build BUILD.syx --out OUT_DIR [--device dn2]

ZERO_PATCHED is tests/emu/zero_patched.rs built as an example of digikit's digi-coupled crate (tests/emu/README.md):
the build runs with the DSP side answering zeros, so the frame interrupt, where the macros apply, runs too. Screens
go to OUT_DIR as PNGs; the exit status is the number of failed checks.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import zlib

GAP = 48_000_000          # runner step budget between panel events: about 12 M instructions (the UI queue lags)
SETTLE = 240_000_000

PAGE, MOD, NO, FUNC, PLAY, STOP = 22, 6, 12, 17, 20, 21
TRIG1, PUSH_A = 25, 41
LED_MOD, LED_TRIG1 = 24, 25   # LED indices, the same on both (digikit devices/digit*-ii-leds.toml)
LEVEL = 9                     # the LEVEL/DATA encoder

# Per device (docs/findings): the frame's per-track values and the mirror they glide to (block + 0x22, stride in
# bytes), where the kit's Sound buffers are, the view on top of the stack on the main screen (D9), the
# configuration's two card sectors, and indices with a non-zero default, to check the Sound arrays against.
DEVICES = {
    'dt2': dict(frame=0x80005b72, mirror=0x80003362, stride=142, view_top=0x40216f14, card=0x2df0e00, probe=(36, 59),
                # the Sound pointers at 0x8000470c (the array at +0x14)
                sounds=lambda r: [r.peek32(0x8000470c + 4 * t) + 0x14 for t in range(16)]),
    'dn2': dict(frame=0x80006906, mirror=0x80003b12, stride=202, view_top=0x402008e8, card=0x7ff000, probe=(67, 90),
                # the kit at *0x800052a0, track t's Sound at + 0x34 + 0x48b t (digikit finding 02, read on 1.11)
                sounds=lambda r: [r.peek32(0x800052a0) + 0x34 + 0x48b * t + 0x14 for t in range(16)]),
}
DEV = DEVICES['dt2']


def delta(pos, depth):
    d = pos * depth * 0x7f00
    return d >> 12 if d >= 0 else -((-d) >> 12)


def oled_png(frame, scale=4):
    """A PNG of the 128 x 64 OLED bytes (byte (7 - y / 8) + 8 x, bit y % 8), as digikit's digi_client draws it."""
    rows = bytearray()
    for y in range(64):
        row = bytearray()
        for x in range(128):
            on = frame[(7 - y // 8) + 8 * x] >> (y % 8) & 1
            row += bytes([255 if on else 0]) * scale
        rows += (b'\x00' + bytes(row)) * scale

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 128 * scale, 64 * scale, 8, 0, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(bytes(rows))) + chunk(b'IEND', b''))


class Run:
    def __init__(self, cmd, out):
        self.p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.out, self.failed, self.n = out, 0, 0
        self.syms = None
        self.sounds = None

    def read_sounds(self):
        """The kit's Sound arrays, taken at boot (on the Digitakt II the pointers hold them only until the sequencer
        first runs). Checked against the mirror, which is a copy: None when they do not match."""
        self.sounds = DEV['sounds'](self)
        if any(self.peek16(self.sounds[t] + 2 * i) != self.peek16(DEV['mirror'] + DEV['stride'] * t + 2 * i)
               for t in (0, 2) for i in DEV['probe']):
            print('SKIP the kit checks: the Sound arrays were not found where the device profile says')
            self.sounds = None

    def cmd(self, line):
        self.p.stdin.write(line + '\n')
        self.p.stdin.flush()
        reply = self.p.stdout.readline().strip()
        if not reply and line != 'leds':
            raise RuntimeError('the runner stopped at: ' + line)
        return reply

    def close(self):
        if self.p.poll() is None:
            self.p.stdin.write('quit\n')
            self.p.stdin.flush()
            self.p.wait(60)

    def step(self, n=GAP):
        self.cmd('step %d' % n)

    def down(self, code):
        self.cmd('down %d' % code)
        self.step()

    def up(self, code):
        self.cmd('up %d' % code)
        self.step()

    def tap(self, code):
        self.down(code)
        self.up(code)

    def turn(self, enc, detents):
        self.cmd('turn %d %d' % (enc, detents))
        self.step(3 * GAP)

    def shot(self, name):
        self.step(SETTLE)
        path = self.out / (name + '.oled')
        self.cmd('frame %s' % path)
        frame = path.read_bytes()
        (self.out / (name + '.png')).write_bytes(oled_png(frame))
        return hashlib.sha256(frame).hexdigest()

    def led(self, idx):
        leds = self.cmd('leds')
        return int(leds[2 * idx:2 * idx + 2], 16) if len(leds) >= 2 * idx + 2 else None

    def peek32(self, addr):
        return int(self.cmd('peek %x' % addr), 16)

    def peek8(self, addr):
        return (self.peek32(addr & ~3) >> (24 - 8 * (addr & 3))) & 0xff

    def peek16(self, addr):
        if addr & 1:                            # the Digitone II's Sound arrays sit at odd addresses
            return self.peek8(addr) << 8 | self.peek8(addr + 1)
        word = self.peek32(addr & ~3)
        return (word >> 16) & 0xffff if addr & 2 == 0 else word & 0xffff

    def values(self, track, index, samples=7):
        """(frame value, mirror value, kit value) of one parameter of one track. The frame block is rebuilt from the
        smoother at the start of each frame and the handlers add to it a little later, so a stop inside that window
        reads the value without them: the frame value is the most common of a few samples taken between steps."""
        seen = []
        for _ in range(samples):
            seen.append(self.peek16(DEV['frame'] + DEV['stride'] * track + 2 * index))
            self.step(30011)
        frame = max(set(seen), key=seen.count)
        kit = self.peek16(self.sounds[track] + 2 * index) if self.sounds else None
        return (frame, self.peek16(DEV['mirror'] + DEV['stride'] * track + 2 * index), kit)

    def mode(self):
        """The mode's state byte (0 off, 1 macro screen, 2 setup), from the build's symbol map; None without one."""
        if not self.syms:
            return None
        addr = self.syms['mm_state']
        word = int(self.cmd('peek %x' % (addr & ~3)), 16)
        return (word >> (24 - 8 * (addr & 3))) & 0xff

    def addr(self, field, *index):
        """The device address of a field of the mode's state (tools/check.sh puts the layout in the map)."""
        return self.syms['mm_state'] + self.syms['layout'][field]

    def pos(self, k):
        L = self.syms['layout']
        v = self.peek8(self.syms['mm_state'] + L['m'] + L['macro'] * k + L['pos'])
        return v - 256 if v > 127 else v

    def slot(self, k, j):
        """(destination, depth, tracks) of slot j of macro k."""
        L = self.syms['layout']
        a = self.syms['mm_state'] + L['m'] + L['macro'] * k + L['slot'] + L['slot_size'] * j
        depth = self.peek8(a + L['depth'])
        return self.peek8(a + L['dest']), depth - 256 if depth > 127 else depth, self.peek16(a + L['tracks'])

    def check(self, ok, what):
        self.n += 1
        if not ok:
            self.failed += 1
        print(('ok   ' if ok else 'FAIL ') + what, flush=True)




def top_view(r):
    """The vtable of the view on top of the firmware's stack, through the Brain the mod saw last; None before a key."""
    brain = int(r.cmd('peek %x' % r.syms['mm_brain']), 16)
    if not brain:
        return None
    sentinel = brain + 0x40 + 0x14
    node = int(r.cmd('peek %x' % (sentinel + 4)), 16)
    view = int(r.cmd('peek %x' % (node + 8)), 16)
    return int(r.cmd('peek %x' % view), 16)


def settle_boot(r):
    """Close the boot's file-system windows: NO until nothing is on top of the main screen's views."""
    for _ in range(60):
        r.tap(NO)
        r.step(SETTLE)
        if r.syms is None:
            r.step(8 * SETTLE)
            r.tap(NO)
            break
        if top_view(r) == DEV['view_top']:
            break
    r.step(SETTLE)


LED_PAGE1 = 19                # TRIG .. FX page keys: LED indices 19 .. 23
LEVEL_PUSH = 49
CONFIG = {}                   # the slot the first run sets up, for the restart part


def chord(r, first, second):
    r.down(first)
    r.down(second)
    r.up(second)
    r.up(first)


def spin(r, enc, total, burst=8):
    """Turn a knob by total counts in fast bursts: the firmware's encoder filter, which the mode's knobs go through
    as the stock ones do, swallows slow single counts (its lead-in) and moves more per count as the turn speeds up."""
    left = total
    while left:
        n = max(-burst, min(burst, left))
        r.turn(enc, n)
        r.step(4_000_000)
        left -= n
    r.step(GAP)


def spin_until(r, enc, sign, done, tries=40):
    for _ in range(tries):
        if done():
            return True
        spin(r, enc, 8 * sign)
    return done()


def affected(tracks):
    return [t for t in (0, 2, 4) if tracks >> t & 1]


def check_frame(r, macro, slot, what):
    """The frame values of tracks 1, 3 and 5 at the slot's destination: mirror + the macro's offset where the slot
    applies, the mirror elsewhere; the kit keeps the mirror."""
    pos = r.pos(macro)
    dest, depth, tracks = r.slot(macro, slot)
    want = delta(pos, depth)
    for t in (0, 2, 4):
        frame, mirror, kit = r.values(t, dest)
        exp = max(0, min(0x7f00, mirror + want)) if tracks >> t & 1 else mirror
        r.check(frame == exp, '%s: track %d, index %d: frame %04x, mirror %04x, expected %04x (pos %d, depth %d)'
                % (what, t + 1, dest, frame, mirror, exp, pos, depth))
        if kit is not None:
            r.check(kit == mirror, '%s: track %d, the kit keeps %04x (mirror %04x)' % (what, t + 1, kit, mirror))


def scenario(r):
    settle_boot(r)
    r.read_sounds()
    stock = r.shot('01-main')
    r.tap(PLAY)                                 # control: the sequencer runs here, its playhead lights the trig keys
    lit = set()
    for _ in range(12):
        r.step(4 * GAP)
        lit |= {t for t in range(16) if r.led(LED_TRIG1 + t) not in (0x00, 0x01)}
    r.tap(STOP)
    r.tap(STOP)
    r.step(SETTLE)
    seq_runs = len(lit) >= 2
    if not seq_runs:
        print('SKIP the playhead checks: the sequencer does not step in this emulator mode (its clock is the audio side)')

    r.tap(8)                                    # D9: not with a menu open (SETTINGS)
    menu = r.shot('01-settings')
    chord(r, PAGE, MOD)
    r.shot('01-settings-chord')                 # MOD itself is the firmware's: it closes the menu for its page
    r.check(menu != stock and r.mode() == 0, 'PAGE + MOD does not start macro mode with a menu open (D9)')
    r.tap(NO)
    r.tap(NO)
    r.tap(2)                                    # back on the SRC page
    r.step(SETTLE)

    pages = [r.led(LED_PAGE1 + i) for i in range(5)]
    chord(r, PAGE, MOD)                         # entry: PAGE held, then MOD
    macro = r.shot('02-macro')
    r.check(r.mode() == 1 and macro != stock, 'PAGE + MOD shows the macro screen')
    r.check(r.led(LED_MOD) == 0x41, 'the MOD LED is blue: %02x' % (r.led(LED_MOD) or 0))
    now = [r.led(LED_PAGE1 + i) for i in range(5)]
    r.check(any(v not in (0, 1) for v in pages) and all(v in (0, 1) for v in now),
            'the page macro mode came from is no longer lit: page LEDs %s, then %s' % (pages, now))

    spin(r, 1, 40)
    spin(r, 3, -40)
    turned = r.shot('03-turned')
    r.check(r.pos(0) > 0 and r.pos(2) < 0 and turned != macro, 'turning A and C moves them: %d, %d' % (r.pos(0), r.pos(2)))

    spin(r, 4, 16)                              # from rest, the same turn: D plain, B pushed
    r.down(PUSH_A + 1)
    spin(r, 2, 16)
    r.up(PUSH_A + 1)
    r.shot('04-coarse')
    r.check(r.pos(1) > r.pos(3) >= 0, 'B pushed moves further than D for the same turn: %d, %d' % (r.pos(1), r.pos(3)))

    r.down(PUSH_A)                              # push A + NO: A back to the centre
    r.tap(NO)
    r.up(PUSH_A)
    r.check(r.pos(0) == 0 and r.pos(1) != 0 and r.mode() == 1, 'push A + NO centres A only')
    r.tap(PUSH_A)
    r.check(r.mode() == 1, 'a push alone does not open the setup')

    r.down(FUNC)                                # FUNC + push C: the setup of macro C
    r.tap(PUSH_A + 2)
    r.up(FUNC)
    setup = r.shot('06-setup')
    r.check(r.mode() == 2 and setup not in (macro, turned), 'FUNC + push C opens the setup screen')
    r.check(r.slot(2, 1)[2] == 0xffff, 'a fresh slot is on every track: %04x' % r.slot(2, 1)[2])

    ok = spin_until(r, 2, 1, lambda: r.slot(2, 1)[0] != 0)  # B: slot 2's destination, in its list
    dest = r.slot(2, 1)[0]
    lst = r.shot('06-list')
    r.check(ok and r.peek8(r.addr('list')) == 2 and lst != setup, 'turning B opens its list and picks index %d' % dest)
    r.tap(NO)
    r.check(r.peek8(r.addr('list')) == 0 and r.mode() == 2 and r.slot(2, 1)[0] == dest,
            'NO closes the list and keeps the choice')
    r.check(spin_until(r, 6, 1, lambda: r.slot(2, 1)[1] >= 10), 'F sets slot 2\'s depth: %d' % r.slot(2, 1)[1])
    r.tap(TRIG1 + 2)                            # tracks 3 and 5 off
    r.tap(TRIG1 + 4)
    r.shot('07-setup-edited')
    tracks = r.slot(2, 1)[2]
    reds = [r.led(LED_TRIG1 + t) for t in range(16)]
    r.check(tracks == 0xffff & ~0b10100, 'trig 3 and 5 take their tracks out: %04x' % tracks)
    # off: 0x01, or 0x00 for an LED the firmware has not sent since the state was restored (it sends changes only)
    r.check(all((v == 0x11) == (t not in (2, 4)) for t, v in enumerate(reds)) and all(v in (0, 1) for v in (reds[2], reds[4])),
            'the trig keys show the slot\'s tracks: %s' % reds)
    if seq_runs:
        playhead_over_setup(r, tracks)

    before = [r.pos(k) for k in range(8)]       # LEVEL on the setup screen: the edited macro only
    spin(r, LEVEL, 24)
    after = [r.pos(k) for k in range(8)]
    r.check(after[2] > before[2] and all(after[k] == before[k] for k in range(8) if k != 2),
            'LEVEL on the setup of C moves C only: %s -> %s' % (before, after))
    r.shot('07-setup-level')
    r.tap(NO)                                   # NO goes back (the list is closed)
    back = r.shot('08-back')
    r.check(r.mode() == 1 and back not in (stock, setup), 'NO goes back from the setup screen to the macro screen')

    spin_until(r, 3, -1, lambda: r.pos(2) <= -6)
    CONFIG.update(dest=dest, depth=r.slot(2, 1)[1], tracks=tracks)
    check_frame(r, 2, 1, 'C at %d' % r.pos(2))
    r.down(PUSH_A + 2)
    r.tap(NO)
    r.up(PUSH_A + 2)
    check_frame(r, 2, 1, 'C at the centre')

    spin_until(r, 3, -1, lambda: r.pos(2) <= -6)
    before = [r.pos(k) for k in range(8)]       # LEVEL on the macro screen: every macro
    spin(r, LEVEL, 24)
    after = [r.pos(k) for k in range(8)]
    moved = {after[k] - before[k] for k in range(8) if -64 < after[k] < 63}
    r.check(len(moved) == 1 and moved.pop() > 0, 'LEVEL moves every macro alike: %s -> %s' % (before, after))
    r.shot('08-level')
    r.tap(LEVEL_PUSH)
    r.check(all(r.pos(k) == 0 for k in range(8)) and r.mode() == 1, 'LEVEL pushed centres every macro')

    spin_until(r, 3, -1, lambda: r.pos(2) <= -6)
    before = r.shot('08-before-reload')
    r.down(FUNC)                                # FUNC + NO: every macro to the centre, no pattern reload
    r.tap(NO)
    r.up(FUNC)
    r.check(all(r.pos(k) == 0 for k in range(8)) and r.mode() == 1, 'FUNC + NO centres the macros and stays')
    check_frame(r, 2, 1, 'after FUNC + NO')
    r.shot('08-func-no')

    spin_until(r, 3, -1, lambda: r.pos(2) <= -6)
    r.tap(3)                                    # a page key: leaves and opens its page
    fltr = r.shot('09-fltr')
    r.check(r.mode() == 0 and fltr != back, 'FLTR leaves macro mode and opens its page')
    check_frame(r, 2, 1, 'after leaving (D2)')
    chord(r, MOD, PAGE)                         # the other order: MOD held, then PAGE
    r.check(r.mode() == 1, 'MOD + PAGE enters too')
    r.tap(MOD)
    r.check(r.mode() == 0 and r.led(LED_MOD) in (0x01, 0x71), 'MOD leaves; the MOD LED is the firmware\'s: %02x'
            % (r.led(LED_MOD) or 0))

    r.down(FUNC)                                # FUNC + NO outside: the firmware reloads the pattern (and keeps FUNC's
    r.tap(NO)                                   # release)
    r.up(FUNC)
    r.step(SETTLE)
    r.shot('10-reloaded')
    chord(r, PAGE, MOD)
    r.check(r.mode() == 1, 'after a pattern reload PAGE + MOD enters again')
    r.tap(PUSH_A)
    r.check(r.mode() == 1, 'and a push alone does not open the setup')
    r.down(FUNC)
    r.tap(PUSH_A)
    r.up(FUNC)
    r.tap(NO)
    r.check(r.mode() == 1, 'FUNC + push and NO: into the setup and back')
    r.tap(MOD)
    r.shot('11-left')
    r.check(r.cmd('card-save %s' % (r.out / 'card.bin')).startswith('ok'), 'the card written so far is saved')


def after_restart(r):
    """A cold boot with the card of the first run: the configuration comes back, the positions do not (D14)."""
    settle_boot(r)
    r.read_sounds()
    chord(r, PAGE, MOD)
    r.shot('20-after-restart')
    dest, depth, tracks = r.slot(2, 1)
    r.check((dest, depth, tracks) == (CONFIG['dest'], CONFIG['depth'], CONFIG['tracks']),
            'after the restart slot 2 of C is back: %s' % ((dest, depth, tracks),))
    r.check(all(r.pos(k) == 0 for k in range(8)), 'and every macro is at the centre')
    spin_until(r, 3, -1, lambda: r.pos(2) <= -6)
    check_frame(r, 2, 1, 'after the restart')
    r.down(FUNC)
    r.tap(PUSH_A + 2)
    r.up(FUNC)
    r.shot('21-setup-after-restart')


def playhead_over_setup(r, tracks):
    """With PLAY running the setup screen's trig keys keep showing the tracks, not the playhead."""
    r.tap(PLAY)
    seen = set()
    for _ in range(12):
        r.step(4 * GAP)
        seen |= {(t, r.led(LED_TRIG1 + t)) for t in range(16)}
    r.tap(STOP)
    r.tap(STOP)
    r.check(all((v == 0x11) == bool(tracks >> t & 1) for t, v in seen),
            'with PLAY running the trig keys keep showing the tracks: %s' % sorted(seen))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runner', required=True)
    ap.add_argument('--stock', required=True)
    ap.add_argument('--build', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--device', choices=sorted(DEVICES), default='dt2')
    ap.add_argument('--map', required=True, help='the build\'s symbol map (tools/check.sh writes out/check/macro-<dev>.map.json)')
    a = ap.parse_args()
    global DEV
    DEV = DEVICES[a.device]
    syms = json.load(open(a.map))
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    state = out / 'build.state'
    state.unlink(missing_ok=True)
    r = Run([a.runner, a.stock, a.build, str(state)], out)
    r.syms = syms
    try:
        scenario(r)
    finally:
        r.close()
    n, failed = r.n, r.failed
    print('-- restart with the card of that run')
    state2 = out / 'restart.state'
    state2.unlink(missing_ok=True)
    # only the mod's two sectors go on a fresh card: the firmware's own writes of the first run (the working project, the
    # +Drive) replayed on a blank card do not make a card it boots from
    saved = (out / 'card.bin').read_bytes()
    ours = b''.join(saved[i:i + 520] for i in range(0, len(saved), 520)
                    if int.from_bytes(saved[i:i + 8], 'big') in (DEV['card'], DEV['card'] + 1))
    (out / 'card-mm.bin').write_bytes(ours)
    r = Run([a.runner, a.stock, a.build, str(state2), str(out / 'card-mm.bin')], out)
    r.syms = syms
    r.check(len(ours) in (520, 1040), 'the first run wrote the record sectors and nothing else of ours (%d bytes)' % len(ours))
    try:
        after_restart(r)
    finally:
        r.close()
    n, failed = n + r.n, failed + r.failed
    print('%d checks, %d failed' % (n, failed))
    return failed


if __name__ == '__main__':
    sys.exit(main())
