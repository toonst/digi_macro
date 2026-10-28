# SPDX-License-Identifier: GPL-2.0-or-later
"""Drive a macro-mode build in digikit's emulator and check the screen, the LEDs and the frame values (PLAN.md 8).

    python3 tests/emu/scenario.py --runner ZERO_PATCHED --stock STOCK.syx --build BUILD.syx --out OUT_DIR

ZERO_PATCHED is tests/emu/zero_patched.rs built as an example of digikit's digi-coupled crate (tests/emu/README.md):
the build runs with the DSP side answering zeros, so the frame interrupt, where the macros apply, runs too. Screens
go to OUT_DIR as PNGs; the exit status is the number of failed checks.
"""
import argparse
import hashlib
import pathlib
import struct
import subprocess
import sys
import zlib

GAP = 48_000_000          # runner step budget between panel events: about 12 M instructions (the UI queue lags)
SETTLE = 240_000_000

PAGE, MOD, NO, FUNC, PLAY, STOP = 22, 6, 12, 17, 20, 21
TRIG1, PUSH_A = 25, 41
LED_MOD, LED_TRIG1 = 24, 25   # LED indices (digikit devices/digitakt-ii-leds.toml)

# DT2 1.17 (docs/findings/s3-frame-values.md): the frame's per-track values, the mirror they glide to, the kit's
# Sound pointers (the Sound array is at +0x14).
FRAME_VALUES, MIRROR, SOUND_PTRS, STRIDE = 0x80005b72, 0x80003362, 0x8000470c, 142


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

    def peek16(self, addr):
        word = int(self.cmd('peek %x' % (addr & ~3)), 16)
        return (word >> 16) & 0xffff if addr & 2 == 0 else word & 0xffff

    def values(self, track, index, samples=7):
        """(frame value, mirror value, kit value) of one parameter of one track. The frame block is rebuilt from the
        smoother at the start of each frame and the handlers add to it a little later, so a stop inside that window
        reads the value without them: the frame value is the most common of a few samples taken between steps."""
        sound = int(self.cmd('peek %x' % (SOUND_PTRS + 4 * track)), 16)
        seen = []
        for _ in range(samples):
            seen.append(self.peek16(FRAME_VALUES + STRIDE * track + 2 * index))
            self.step(30011)
        frame = max(set(seen), key=seen.count)
        return (frame, self.peek16(MIRROR + STRIDE * track + 2 * index), self.peek16(sound + 0x14 + 2 * index))

    def check(self, ok, what):
        self.n += 1
        if not ok:
            self.failed += 1
        print(('ok   ' if ok else 'FAIL ') + what, flush=True)


def scenario(r):
    r.tap(NO)                                   # the emulator's FILE SYSTEM OK window
    stock = r.shot('01-main')
    led_mod_stock = r.led(LED_MOD)

    r.down(PAGE)                                # entry: PAGE held, then MOD
    r.down(MOD)
    r.up(MOD)
    r.up(PAGE)
    macro = r.shot('02-macro')
    r.check(macro != stock, 'PAGE + MOD shows the macro screen')
    r.check(r.led(LED_MOD) == 0x41, 'the MOD LED is blue: %02x' % (r.led(LED_MOD) or 0))

    r.turn(1, 10)
    r.turn(3, -6)
    turned = r.shot('03-turned')
    r.check(turned != macro, 'turning A and C moves their knobs')

    r.down(PUSH_A + 1)                          # push B + turn: bigger steps
    r.turn(2, 2)
    r.up(PUSH_A + 1)
    r.shot('04-coarse')

    r.down(PUSH_A)                              # push A + NO: A back to the centre
    r.tap(NO)
    r.up(PUSH_A)
    r.shot('05-reset-a')

    r.down(FUNC)                                # FUNC + push C: the setup of macro C
    r.tap(PUSH_A + 2)
    r.up(FUNC)
    setup = r.shot('06-setup')
    r.check(setup not in (macro, turned), 'FUNC + push C opens the setup screen')

    r.turn(2, 3)                                # B: slot 2 destination (index 27)
    r.turn(6, -12)                              # F: slot 2 depth
    r.tap(TRIG1 + 2)
    r.tap(TRIG1 + 4)
    r.shot('07-setup-edited')
    reds = [r.led(LED_TRIG1 + t) for t in range(16)]
    r.check(reds[2] == 0x11 and reds[4] == 0x11, 'trig keys 3 and 5 are red: %s' % reds)
    r.check(all(v == 0x01 for t, v in enumerate(reds) if t not in (2, 4)), 'the other trig keys are off')

    r.tap(MOD)                                  # MOD on the setup screen: back to the macro screen (D8)
    back = r.shot('08-back')
    r.check(back not in (stock, setup), 'MOD goes back from the setup screen to the macro screen')

    want = delta(-6, -12)                       # macro C at -6: index 27 by depth -12 on tracks 3 and 5 only
    for t in (2, 4, 0):
        frame, mirror, kit = r.values(t, 27)
        exp = max(0, min(0x7f00, mirror + want)) if t in (2, 4) else mirror
        r.check(frame == exp, 'track %d, index 27: frame %04x, mirror %04x, expected %04x' % (t + 1, frame, mirror, exp))
        r.check(kit == mirror, 'track %d, index 27: the kit keeps %04x (mirror %04x)' % (t + 1, kit, mirror))
    r.turn(3, 6)                                # C back to the centre: the frame values follow the mirror again
    for t in (2, 4):
        frame, mirror, _ = r.values(t, 27)
        r.check(frame == mirror, 'track %d at the centre: frame %04x = mirror %04x' % (t + 1, frame, mirror))
    r.turn(3, -6)

    r.tap(MOD)                                  # a second MOD leaves macro mode
    left = r.shot('09-left')
    r.check(left != back, 'MOD leaves macro mode')
    r.check(r.led(LED_MOD) == led_mod_stock, 'the MOD LED is the firmware\'s again: %02x' % (r.led(LED_MOD) or 0))
    frame, mirror, _ = r.values(2, 27)
    r.check(frame == max(0, min(0x7f00, mirror + want)), 'the macro keeps working after leaving the mode (D2)')

    r.down(MOD)                                 # the other order: MOD held, then PAGE
    r.down(PAGE)
    r.up(PAGE)
    r.up(MOD)
    again = r.shot('10-mod-then-page')
    r.check(again != left, 'MOD + PAGE enters too')
    r.tap(MOD)
    r.shot('11-left-again')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--runner', required=True)
    ap.add_argument('--stock', required=True)
    ap.add_argument('--build', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    state = out / 'build.state'
    state.unlink(missing_ok=True)
    r = Run([a.runner, a.stock, a.build, str(state)], out)
    try:
        scenario(r)
    finally:
        r.close()
    print('%d checks, %d failed' % (r.n, r.failed))
    return r.failed


if __name__ == '__main__':
    sys.exit(main())
