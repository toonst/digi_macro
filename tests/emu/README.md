# Emulator scenarios

The builds run in [digikit](https://github.com/toonst/digikit)'s emulator. Its daemon (`digi-emud`) refuses a build it
does not know at start and loads a patched one ColdFire-only, where the frame interrupt (and so the macros) never runs.
digikit's `zero_patched` runner (`packages/emulator/coupled/examples/zero_patched.rs`, on its
`claude/zen-rubin-g8jh03` branch until it is merged; digikit's docs/INSTRUMENTATION.md, "Scripted patched runs") runs
a patched build with the DSP side answering zeros instead, so the frame interrupt runs. Build it once, in a digikit
checkout (it needs the Rust toolchain digikit's `mise.toml` names):

```bash
cargo build --manifest-path DIGIKIT/Cargo.toml --release -p digi-coupled --example zero_patched
```

Then, with a build from `tools/check.sh`:

```bash
python3 tests/emu/scenario.py --runner DIGIKIT/target/release/examples/zero_patched --stock firmware/Digitakt_II_OS1.17.syx --build out/check/macro-dt2.syx --out out/emu --map out/check/macro-dt2.map.json
```

For the Digitone II, add `--device dn2` and use its stock file, build and map. `--map` is the symbol map `tools/check.sh` writes next to the build: with it the scenario reads the mode's state and
the firmware's view stack (the D9 check). The runner first cold-boots the build to its main screen and saves that state next to the screens (`build.state`,
about 3 s); every run starts from a fresh one. The second part restarts with the card sectors the first part wrote
(`card-mm.bin`) and checks that the configuration came back. The screens are `out/emu/*.png`. The zero-DSP runner produces no sound:
the frame values are checked in memory.

With the same arguments, the kits feature (a build with it, the default):

- `tests/emu/project.py` (23 checks): the PERSONALIZE row, a setup following the kit on, the kits' and the
  project's setups kept apart, pattern copy and paste, SAVE PROJECT AS and LOAD PROJECT.
- `tests/emu/compat.py` (12 checks): projects round-trip between the mod and the stock OS (the stock `.syx` runs
  as the build) and differ only in the bytes core keeps mods' data in; a project with someone else's bytes in them is a
  conflict, and core leaves those bytes alone (docs/DEVELOPING.md, "Storage and other mods").

The runner's other commands (`dump`, memory guards, register logs, `run-until`) are in digikit's
docs/INSTRUMENTATION.md.
