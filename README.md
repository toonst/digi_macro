# macro-mode

A performance macro mode for the **Digitakt II (OS 1.17)** and **Digitone II (OS 1.12)**, as an
[elekloader](https://github.com/elekloader/elekloader) mod. Hold PAGE and press MOD (or the other way round): encoders A–H
become eight macros, each moving up to four parameters on the tracks you choose, on top of the sound like the stock
modulations. Requirements: [SPEC.md](SPEC.md); plan and decisions: [PLAN.md](PLAN.md); where it stands:
[docs/STATUS.md](docs/STATUS.md).

> **Development build.** On both devices everything runs in digikit's emulator (screens, LEDs, the macros in the
> frame values, the stored configuration), but nothing has been tried on a unit or listened to yet. Do not flash it
> on a device you need.

## Using it

| | Macro screen | Setup screen |
| --- | --- | --- |
| Enter | Hold PAGE and press MOD, or hold MOD and press PAGE | FUNC + push an encoder (A–H) |
| Encoders A–H | Move that macro (−64 … +63); push and turn for bigger steps | A–D: destination of parameter 1–4; E–H: its depth |
| Push + NO | The macro back to the centre | That knob back: destination none, depth 0 |
| Trig keys | Their usual function | The tracks of the selected parameter (red) |
| LEFT / RIGHT | Their usual function | Select a parameter |
| NO | Its usual function | Back to the macro screen |
| LEVEL/DATA | Move all eight macros together | Move the macro being edited |
| FUNC + NO | All eight macros back to the centre (the pattern is not reloaded) | The same |
| MOD | Leave macro mode (also with PAGE held) | Back to the macro screen |

PLAY, STOP, RECORD, PTN, SONG, TRK and FUNC keep their stock function throughout.

![The macro screen](docs/img/macro-turned.png) ![The setup screen of macro C](docs/img/setup.png)

## Building

You need Python 3, a host C compiler and the m68k cross tools, and your own stock OS files from Elektron's site.
This repository never holds firmware. The cross tools:

- Ubuntu or Debian: `apt install gcc-m68k-linux-gnu binutils-m68k-linux-gnu`
- macOS: `brew install m68k-elf-gcc` (it brings `m68k-elf-binutils`)

`tools/check.sh` uses whichever of `m68k-linux-gnu-` and `m68k-elf-` it finds (`ELEKLOADER_CROSS` names another
prefix), and runs on the bash macOS ships.

```bash
git submodule update --init
```

```bash
DT2_SYX=path/to/Digitakt_II_OS1.17.syx DN2_SYX=path/to/Digitone_II_OS1.12.syx tools/check.sh
```

`tools/check.sh` runs the host tests, renders the screen previews (`--previews` rewrites `docs/img`), cross-compiles
both manifests and, with the stock files, builds the elekloader core and this mod for each device and patches them
into the stock OS: `out/check/macro-dt2.syx` and `out/check/macro-dn2.syx`. Those are the files to send to a device
(never commit them: they hold Elektron's firmware).

Test a build in [digikit](https://github.com/toonst/digikit)'s emulator before it goes near a device
(`mise run workbench -- /path/to/out/check/macro-dt2.syx` in a digikit checkout).

## Layout

| Path | What |
| --- | --- |
| `src/mm.h` | Types and the interface of the device-independent part |
| `src/mm_logic.c` | States and the input map: entry and exit, the macro screen, the setup screen |
| `src/mm_apply.c` | Macro math, the per-track lists and their frame-time apply; the LEDs the mode holds |
| `src/mm_store.c` | The stored configuration's format; destination helpers |
| `src/mm_draw.c` | The two screens, in the stock layout (the previews use the mod's own font; the device the firmware's) |
| `src/mm_dests.c` | The destinations per device: both from the firmware's parameter tables |
| `src/mm_glue.c` | The elekloader handlers: `ev_key`, `ev_enc`, `ev_tick`, `ev_draw` |
| `mods/dt2`, `mods/dn2` | The elekloader manifests, one per device and OS |
| `tests/` | Host tests and the preview renderer |
| `third_party/elekloader` | elekloader, pinned |

## Licence

GPL-2.0-or-later ([LICENSE](LICENSE)), as elekloader. Not affiliated with Elektron; flashing custom firmware is at
your own risk. The stock OS file always restores a unit (hold FUNC while powering on, TRIG 4, then send the stock
`.syx` over DIN MIDI).
