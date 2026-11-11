# Emulator scenarios

The builds run in [digikit](https://github.com/toonst/digikit)'s emulator. Its daemon (`digi-emud`) refuses a build it
does not know at start and loads a patched one ColdFire-only, where the frame interrupt (and so the macros) never runs.
The `zero_patched` runner (`packages/emulator/coupled/examples/zero_patched.rs`; docs/INSTRUMENTATION.md, "Scripted
patched runs") runs a patched build with the DSP side answering zeros instead, so the frame interrupt runs. It is in
toonst/digikit's `claude/zen-rubin-g8jh03` branch (`3d3a243`) and stays there: upstream digikit declined it
(ElekLabs/digikit#215), so check out that branch, not upstream's main. Build it once, in a digikit
checkout (it needs the Rust toolchain digikit's `mise.toml` names, and on Linux the ALSA headers: `libasound2-dev`):

```bash
cargo build --manifest-path DIGIKIT/Cargo.toml --release -p digi-coupled --example zero_patched
```

Then, with a build from `tools/build.sh`:

```bash
python3 tests/emu/scenario.py --runner DIGIKIT/target/release/examples/zero_patched --stock firmware/Digitakt_II_OS1.17.syx --build out/macro-dt2.syx --out out/emu/scenario-dt2 --map out/build/macro-dt2.map.json
```

For the Digitone II, add `--device dn2` and use its stock file, build and map. `--map` is the symbol map `tools/build.sh` writes in `out/build`: with it the scenario reads the mode's state and
the firmware's view stack (the D9 check). The runner first cold-boots the build to its main screen and saves that state next to the screens (`build.state`,
about 3 s); every run starts from a fresh one. The second part restarts with the card sectors the first part wrote
(`card-mm.bin`) and checks that the configuration came back. The screens are `out/emu/scenario-dt2/*.png`. The zero-DSP runner produces no sound:
the frame values are checked in memory.

With the same arguments, the kits feature (a build with it, the default):

- `tests/emu/project.py` (23 checks): the PERSONALIZE row, a setup following the kit on, the kits' and the
  project's setups kept apart, pattern copy and paste, SAVE PROJECT AS and LOAD PROJECT.
- `tests/emu/project_only.py` (23 checks), for a build made with `MM_KITS=project`: the setup and the knobs in the
  project's header, nothing in the kit records or on the mod's card sectors, and LOAD PROJECT bringing them back.
- `tests/emu/compat.py` (12 checks): projects round-trip between the mod and the stock OS (the stock `.syx` runs
  as the build) and differ only in the bytes core keeps mods' data in; a project with someone else's bytes in them is a
  conflict, and core leaves those bytes alone (docs/DEVELOPING.md, "Storage and other mods").
- `tests/emu/compat_project.py` (13 checks): the same round trips and conflict for a build made with
  `MM_KITS=project`, whose only bytes in the file are the header's gap. On such a build `scenario.py` fails its three
  restart checks (they look for the card record this build does not keep) and `compat.py` its kit checks.
- `tests/emu/frame_load.py`: what the mod adds to the frame interrupt, in instructions (not time): the interrupt's
  length on the stock OS and with the mod, and the hook's own, with no macro, one macro and all eight on four
  destinations and sixteen tracks each. It writes `frame-load.json`.

The runner's other commands (`dump`, memory guards, register logs, `run-until`) are in digikit's
docs/INSTRUMENTATION.md.
