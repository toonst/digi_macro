# S3: where the macros apply (Digitakt II 1.17, Digitone II 1.12)

Spike S3 of [PLAN.md](../../PLAN.md): a place where the macros add to the sound **on top**, like the stock modulators
(D1), without changing the kit. Marks as digikit's: **[D]** read or measured once here, **[O]** open, **[V]** checked
by a second agent against the image bytes (nothing here is **[V]** yet).

## Result: go on the Digitakt II **[D]**

The frame interrupt (vector 191) builds a fresh block of per-track parameter values every frame, and the stock
modulators add to that block. A hook just before them adds the macros the same way: the kit, the Sound arrays and the
mirror are not touched, and with every macro at the centre nothing is written.

## The frame path on 1.17 **[D]**

Read with `m68k-linux-gnu-objdump` on the MAIN image (`elekloader.formats.main_image`); the addresses are 1.17's.

| Address | What |
| --- | --- |
| `0x4002e8c6` | `jsr 0x400d9406` with `0x80003340` pushed, in the vector-191 handler: returns the frame block `0x80005b50` |
| `0x400d9406` | Glides every value of the mirror (`0x80003340 + 0x22 + 142 t`, the copy of each track's Sound array) toward its target with the EMAC (state at `0x8000dd40`) and writes the result into the frame block, **every frame**: the block never accumulates |
| `0x4002e8d8`..`0x4002e908` | Per-track fix-ups of the block (`+0x5a`..`+0x5e` of each 142-byte track row) |
| `0x4002e90c` | `jsr 0x400d9508` with the block pushed: **the hook site** (6 bytes, one whole instruction) |
| `0x400d9508` | The per-track modulators: for each track, velocity (`0x800047dc[t]`) and a second source (`0x8000483c[t]`, `>> 8`) through four destinations each, into the block at `+0x22 + 142 t` |
| `0x400d94b8` | The 4-destination kernel: `msacw` per destination (`depth << 16 | dest`), then saturates around `0x4000_0000` and writes the value back (digikit finding 04's kernel, 1.16 `0x400d9354`) |
| `0x4002e91c` | `jsr 0x4013961e`: the LFO tick (1.16 `0x40139342`), on the same block |

So track `t`, value-array index `j`, sits at `0x80005b72 + 142 t + 2 j` during the frame: the index the stock
modulation destinations and the Sound array use.

## The Digitone II 1.12 **[D]**

The frame interrupt calls the glide `0x400db28e` (mirror `0x80003af0`, block `0x800068e4`) at `0x4002717e`, then the
per-track modulators `0x400db390` at `0x40027196`, with the block and the mirror pushed. Track rows are 202 bytes
(101 values), the values again at `+0x22`: track `t`, index `j` at `0x80006906 + 202 t + 2 j`. The hook is the same
(`src/frame.s`, `FRAME_STRIDE` 101); the stock driver keeps its second argument.

## The hook **[D]**

`src/frame.s` replaces the `jsr` at `0x4002e90c` with a `jsr` to `mmx_frame`, which calls each handler of the
collection `ev_frame_params` as `f(int16_t *values, int stride)` with `values = block + 0x22`, `stride = 71`, then jumps
to `0x400d9508` with the stack as it came. So the stock velocity, second source and LFOs ride on top of the macros, as
on elekloader's Digitone mk1 `ev_render_params`. The handler (`mm_ev_frame_params`) adds the precomputed per-track
deltas and holds each value in its destination's range (`0 .. 0x7f00` for most;
[s6](s6-destinations.md)).

## Measured in the emulator **[D]**

`tests/emu/scenario.py` with the zero-DSP runner (digikit's emulator, the DSP side answering zeros so the frame
interrupt runs), on an elekloader build of core-dt2 1.0 and this mod:

- macro C at −6, slot 2 on index 34 (LEV) with depth −12 on tracks 3 and 5: the frame values of those tracks are
  `mirror + 0x023b` (= `(−6 × −12 × 0x7f00) >> 12`), track 1's equal the mirror, and every kit value equals the
  mirror;
- C back at the centre: the frame values equal the mirror again;
- after leaving macro mode the offset stays (D2).

A sample taken between the block's rebuild and the hook reads the value without the macros; the scenario takes the
most common of seven samples.

On the Digitone II, macro C on FREQ (67) with depth +12 moves tracks 3 and 5 from `0x7f00` to `0x7cc5`; the kit
(at `*0x800052a0 + 0x34 + 0x48b t + 0x14`, digikit finding 02's 1.11 chain, the same on 1.12) keeps `0x7f00`.

## Open **[O]**

- Whether the DSP smooths a value that jumps at the UI
  tick (zipper noise; PLAN.md section 9).
- The hook's cost per frame, and whether the second source at `0x8000483c` is aftertouch.
- Audio: the zero-DSP runner produces no sound. The check against the DSP needs digikit's coupled emulator (its ready
  inputs need the public ADSP-2156x hardware reference, which this environment could not download) or a unit.
