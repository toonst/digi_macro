# Developing macro-mode

How the mod is built, checked and laid out. For what it does and how to use it, see the [README](../README.md).

The mod is an [elekloader](https://github.com/elekloader/elekloader) format-2 mod. It needs core-dt2 1.1 (Digitakt II)
and core-dn2 0.3 (Digitone II), which are in
[a fork](https://github.com/toonst/elekloader/tree/claude/zen-rubin-g8jh03) until elekloader takes them; the submodule
`third_party/elekloader` points there. Requirements: [SPEC.md](../SPEC.md); plan and decisions: [PLAN.md](../PLAN.md);
where it stands: [STATUS.md](STATUS.md); firmware findings: [findings/](findings/).

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

To patch by hand instead (for another version string, say), run elekloader from this directory with the submodule on
Python's module path:

```bash
PYTHONPATH=third_party/elekloader python3 -m elekloader.patch --stock path/to/Digitakt_II_OS1.17.syx --mod out/check/dt2-core/core-1.1.elemod --mod out/check/dt2-mod/macro-mode-0.1.elemod --out out/macro-dt2.syx --version MM01
```

### Features

The mod's optional parts are built in or left out when it is built. Each is a fragment of the manifest,
`mods/<dev>/<feature>.json`, that `tools/manifest.py` adds to `mods/<dev>/mod.json` (sources, flags, firmware sites,
events), and a header in `src/` that the rest of the mod calls through, empty without the feature. The one there is:

| Feature | Fragment | Left out with | What it adds |
| --- | --- | --- | --- |
| Kits | `kits.json`, `src/mm_kits.h` | `MM_KITS=0 tools/check.sh` | Setups per project or per kit, PERSONALIZE > MACRO SETUP, in core's kit and project data (about 25 KB of RAM) |

Without it the mod keeps one setup, on the card.

### Storage and other mods

The mod hooks no firmware code of its own: elekloader's core gives it the frame values, the LEDs, the PERSONALIZE row
and the kit and project storage. Where it keeps its data:

| What | Where | Claim |
| --- | --- | --- |
| The mode's state, every kit's and the project's setup while the unit runs | The mod's own RAM (its `.bss`, placed by elekloader's linker) | |
| The configuration (the setup on, the MACRO SETUP choice) | Two card sectors the stock OS never writes | `card:0x2df0e00-0x2df0e02` (DT2), `card:0x7ff000-0x7ff002` (DN2) |
| A kit's setup, the project's setup (kits feature) | core's kit data (after each kit record's end mark) and project data (the project header's gap) | `kitdata:MMKS`, `projdata:MMPS` |

- **Compatible either way.** core's areas are bytes the stock OS writes as zeros and never reads, and keeps when it
  saves a project it loaded. The stock OS loads and saves a project the mod saved, and the files differ only there;
  the mod loads a stock OS's project and finds no setups. Patterns and kits are the project's records, so the same
  holds for them.
- **Claimed.** elekloader's linker refuses another mod that claims the same names, a card range that overlaps
  ours, or a site on core's.
- **Guarded.** core writes its areas only over zeros or its own data. Anything else there (another tool's, a later
  OS's) is left as it is; the mod then keeps no setup for that kit and shows MACRO SETUP CONFLICT in the header.

On the Digitakt II a kit record has room for one kit-data block of the setup's size (191 bytes): another kit-data mod
would leave one of the two out.

Checks:

```bash
MM_WITH="path/to/other.elemod path/to/another/mod.json" tools/check.sh
```

`tools/storage.py` (run by `tools/check.sh`) checks the claims against the card sectors the code writes, and, with
`MM_WITH`, against those mods from their manifests alone; with the stock files elekloader's lint also links the mod
with MM_WITH's `.elemod` files. `tests/test_storage.py` tests the checker on the fixtures in `tests/storage`. In the
emulator ([tests/emu/README.md](../tests/emu/README.md)), `tests/emu/compat.py` covers the round trips and a conflict:

```bash
python3 tests/emu/compat.py --runner RUNNER --stock path/to/Digitakt_II_OS1.17.syx --build out/check/macro-dt2.syx --map out/check/macro-dt2.map.json --out out/emu-compat-dt2 --device dt2
```

`MM_LED_TEST=1 tools/check.sh` builds a colour test: on the macro screen trig key n lights with value
`(n − 1) × 0x10 + 1` and TRIG, SRC, FLTR, AMP, FX with `0x11`, `0x21`, `0x31`, `0x51`, `0x61`, to tell which LED value
is which colour on a unit.

Test a build in [digikit](https://github.com/toonst/digikit)'s emulator before it goes near a device
(`mise run workbench -- /path/to/out/check/macro-dt2.syx` in a digikit checkout).

## Layout

| Path | What |
| --- | --- |
| `src/mm.h` | Types and the interface of the device-independent part |
| `src/mm_logic.c` | States and the input map: entry and exit, the macro screen, the setup screen |
| `src/mm_apply.c` | Macro math, the per-track lists and their frame-time apply; the LEDs the mode holds |
| `src/mm_store.c` | The stored configuration's format; destination helpers |
| `src/mm_kits.h`, `src/mm_kits.c` | The kits feature: its interface to the glue, and its device side (core's kit and project data, the PERSONALIZE row) |
| `src/mm_scope.c` | The kits feature's logic: setups per project or per kit, following the kit on |
| `src/mm_draw.c` | The two screens, in the stock layout (the previews use the mod's own font; the device the firmware's) |
| `src/mm_dests.c` | The destinations per device: both from the firmware's parameter tables |
| `src/mm_glue.c` | The elekloader handlers: `ev_key`, `ev_enc`, `ev_tick`, `ev_draw`, `ev_frame_params`, `ev_key_leds` |
| `mods/dt2`, `mods/dn2` | The elekloader manifests, one per device and OS (`mod.json`), and each feature's fragment |
| `tools/manifest.py` | The manifest a build uses: `mod.json` and the chosen features |
| `tools/storage.py` | The mod's storage claims, against the card sectors it writes and against other mods |
| `tests/` | Host tests, the preview renderer and the emulator scenarios (`tests/emu`) |
| `third_party/elekloader` | elekloader with core-dt2 1.1 and core-dn2 0.3 (the fork), pinned |
