# Emulator scenarios

The builds run in [digikit](https://github.com/toonst/digikit)'s emulator. Its daemon (`digi-emud`) refuses a build it
does not know at start and loads a patched one ColdFire-only, where the frame interrupt (and so the macros) never runs.
`zero_patched.rs` runs a patched build with the DSP side answering zeros instead, so the frame interrupt runs.

Once, in a digikit checkout (it needs the Rust toolchain digikit's `mise.toml` names):

```bash
cp tests/emu/zero_patched.rs DIGIKIT/packages/emulator/coupled/examples/
```

```bash
cd DIGIKIT && cargo build --release -p digi-coupled --example zero_patched
```

Then, with a build from `tools/check.sh`:

```bash
python3 tests/emu/scenario.py --runner DIGIKIT/target/release/examples/zero_patched --stock firmware/Digitakt_II_OS1.17.syx --build out/check/macro-dt2.syx --out out/emu
```

The runner first cold-boots the build to its main screen and saves that state next to the screens (`build.state`,
about 3 s); every run starts from a fresh one. The screens are `out/emu/*.png`. The zero-DSP runner produces no sound:
the frame values are checked in memory.
