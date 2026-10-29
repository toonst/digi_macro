# Status

Against [PLAN.md](../PLAN.md) section 7. Measured on the Digitakt II 1.17 and the Digitone II 1.12 in digikit's
emulator with `tests/emu/scenario.py` (68 checks each), `tests/emu/project.py` (23) and `tests/emu/compat.py` (12),
all passing; host tests with `tools/check.sh`.

| Part | State |
| --- | --- |
| Mode logic: entry and exit (D15, D8, D9), input map (D3, D6, D7, D12, D16), release follows press | Done, host-tested; in the emulator on DT2 and DN2 |
| Macro math, per-track lists, frame-time apply held in each destination's range (W2) | Done; in the emulator on DT2 and DN2 ([s3](findings/s3-frame-values.md)) |
| Screens in the stock layout, firmware font (S4) | Done on DT2 and DN2 ([s4](findings/s4-screens.md)); previews in `docs/img` use the mod's own font |
| Destinations with the firmware's names (S6, W4) | DT2: 28 per-track destinations, DN2: 26 ([s6](findings/s6-destinations.md)) |
| LEDs (W1) | Done on DT2 and DN2 ([s2](findings/s2-leds.md)) |
| Stored configuration (W3) | Done on DT2 and DN2 ([s5](findings/s5-storage.md)) |
| LEVEL/DATA and FUNC + NO (D16) | Done on DT2 and DN2 ([s1](findings/s1-input.md)) |
| Header title, naming macros, TRK + trig, firmware views over the mode, stock depth format | Done on DT2 and DN2 ([s1](findings/s1-input.md), [s4](findings/s4-screens.md)) |
| Setups per project or per kit, kept apart, saved in the project; copies take them along; PERSONALIZE > MACRO SETUP (D4); an optional feature | Done on DT2 and DN2 ([s7](findings/s7-project-storage.md)) |

Still open:

| Where | Now | To do |
| --- | --- | --- |
| Machine-dependent source parameters (all of DN2's), LFO parameters | Not offered | S6 |
| Audio | The zero-DSP runner makes no sound | Coupled emulator or a unit |
| Storage claims, other mods | Claimed ranges, checked by `tools/storage.py` (with `MM_WITH`); a guard at run time | elekloader compares claims as names: ranges could move there (S7) |
| Kit manager, PASTE KIT, undo | Not driven in the emulator (pattern copy and paste is) | Find their keys and add them to `tests/emu/project.py` (S7) |
