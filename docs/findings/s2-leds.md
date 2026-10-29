# S2: the key LEDs (Digitakt II 1.17, Digitone II 1.12)

Spike S2 of [PLAN.md](../../PLAN.md): hold the MOD LED blue in macro mode and show the selected parameter's tracks red
on the trig keys in the setup screen, over whatever the stock views set. Marks as digikit's (**[D]** read or measured
once here, **[O]** open, **[V]** checked by a second agent; nothing here is **[V]** yet).

## Result **[D]**

The Digitakt II composes its key LEDs the way elekloader's Digitone mk1 does (core-dn1 3.3, `ev_key_leds`): each LED
frame clears every LED, the views set theirs, and the first setter of an LED wins. A hook right after the clear lets a
handler set LEDs before the views; theirs are refused.

## The LedManager on 1.17 **[D]**

Read with `m68k-linux-gnu-objdump` on the MAIN image. The getter of the singleton is `0x40199c02`.

| Address | What |
| --- | --- |
| `0x400308e0` | `set(mgr, logical, layer, value)`: refuses an id `0x40133ef2` rejects, and an LED whose layer (`mgr + 0x28 + 4 logical`) is already set; with layer 0 it skips a value equal to the current one (`mgr + 0xf0 + 4 logical`); stores layer and value, and layer 1 forces the value to 1 (off); then the record builder `0x40134040` |
| `0x4003094a` | `set(mgr, logical, on)`: layer 2 when on, 1 when off |
| `0x4003095a` | Whether an LED's layer is set |
| `0x4003098e` | Clear: every layer and value to 0 |
| `0x400309b6` | Finish: every LED 1..49 whose layer is still 0 is set off (layer 1), then the flush `0x40133da4` |
| `0x4003117c` | The LED frame, `f(brain)`: when the view controller's byte `+0x21` is set (`0x4011bc76`), get the manager, clear it (`jsr (pc)` at `0x400311a2`), let the views set theirs (`0x4011bd1e(ctrl)`), finish |
| `0x4011bc80` | Sets the view controller's byte `+0x21`: asks for an LED frame |

The logical id of a key's LED is its index in digikit's `devices/digitakt-ii-leds.toml` plus 1: MOD 25, TRIG n 25 + n
(digikit finding 19).

## The Digitone II 1.12 **[D]**

The same code, found by the Digitakt II's bytes with the addresses masked: setter `0x4002c318`, clear `0x4002c3c6`,
finish `0x4002c3ee`, the LED frame `0x4002c854` with the hook site at `0x4002c87a` (`4eba fb4a 2f02`), the request
byte's setter `0x4011d5f4`. The logical ids are the same (digikit's `digitone-ii-leds.toml`).

## The hook **[D]**

elekloader's core (core-dt2 1.1, core-dn2 0.3: its `mods/core-dt2/leds.s`, first built here) replaces the 6 bytes at
the site (DT2 `0x400311a2`) (the 4-byte `jsr (pc)` to the clear and the `move.l d2,-(sp)` after it) with a `jsr` to
`core_key_leds`: it runs the clear, calls each handler of the event `ev_key_leds` as
`f(void *mgr)`, and replays the displaced push. `mm_ev_key_leds` sets MOD to `0x31` (blue) with layer 2, and in the setup
screen each trig key to `0x11` (layer 2) or off (layer 1). When the mode's LEDs change, the tick sets the view
controller's `+0x21` so an LED frame runs.

## Measured in the emulator **[D]**

`tests/emu/scenario.py` with the zero-DSP runner: MOD reads `0x31` in macro mode and the firmware's own value after
leaving it; in the setup screen trig keys 3 and 5 read `0x11` and the others off. The same on the Digitone II.

## Colours, seen on a unit **[V on hardware]**

The value's high nibble is a palette index, not a colour mask (digikit finding 19 infers bits for red, green and
blue). With the colour test (`MM_LED_TEST=1 tools/check.sh`: trig key n lit with `(n - 1) << 4 | 1`) on a Digitakt
II, the owner saw:

| Value | Colour |
| --- | --- |
| `0x01` | off |
| `0x11` | red |
| `0x21` | green |
| `0x31` | deep blue |
| `0x41` | yellow |
| `0x51` | light blue |
| `0x61` | purple |
| `0x71` | white |
| `0x81` | orange (close to red) |

So the page keys' `0x71` is white and RECORD's `0x11` red, as before; `0x41` (the mode's first "blue") is yellow and
`0x81` (its second) orange. The mode lights MOD with `0x31`. `0x91` .. `0xf1` were not reported.

## Open **[O]**

- The same palette on a Digitone II (its LEDs take the same values).
- The playhead: the sequencer does not step in the zero-DSP runner, so whether a running pattern's playhead stays off
  the setup screen's trig keys is checked by the scenario only where the sequencer runs (coupled emulator, unit).
