# Status

Against [PLAN.md](../PLAN.md) section 7.

| Part | State |
| --- | --- |
| Mode logic: entry and exit (D15, D8), input map (D3, D6, D7, D12), release follows press | Done, host-tested (`tests/test_mm.c`) |
| Macro math, per-track lists, frame-time apply, saturation | Done, host-tested; not yet called by the firmware (W2) |
| Screens: macro and setup, own font and knob | Done, host-tested; previews in `docs/img` |
| Stored configuration format | Done, host-tested; not yet stored (W3) |
| elekloader glue and manifests (DT2 1.17, DN2 1.12) | Written; cross-compile clean (no undefined symbols, about 7.5 KB of code). **Not yet built by elekloader**: that needs the stock OS files |
| M1 walking skeleton in the emulator | Needs the stock OS files and digikit's emulator |
| P0 spikes S1–S6 | Need the stock OS files and digikit's analysis databases |
| W1 LEDs, W2 frame values, W3 storage, W4 real destinations | After the spikes |

Provisional choices the spikes settle:

| Where | Choice now | Spike |
| --- | --- | --- |
| `MM_COARSE` | Push + turn moves 8 steps | S1: the stock step |
| `MM_ENC_LEVEL` | LEVEL is `ev_enc` id 9 | S1 |
| `can_enter` | Entry allowed on every screen | S1 / S4: D9, no menu or dialog open |
| `MM_LED_*` | Blue `0x41`, red `0x11`, off `0x01` | S2 |
| `src/mm_dests.c` | Value-array indices 25 up to the modulation bound, six named | S6 |
| Font and knob | The mod's own | S4 |
