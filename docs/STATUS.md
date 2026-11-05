# Status

Against [PLAN.md](../PLAN.md) section 7. Measured on the Digitakt II 1.17 in digikit's emulator (its `zero_patched`
runner) on 5 November 2026, with this repository at bc2295e on core-dt2 1.1 as elekloader#221 now has it (0df8409):
`tests/emu/scenario.py` 94 checks, `tests/emu/project.py` 32 and `tests/emu/compat.py` 12, all passing; host tests
with `tools/check.sh`. On a unit: the same build (tools/build.sh, core 0df8409) runs on a Digitakt II, OS 1.17, as toonst
reported on 5 November 2026. The Digitone II (68, 23 and 12 on the earlier core-dn2 0.3) is not built now (DEVELOPING.md).

| Part | State |
| --- | --- |
| Mode logic: entry and exit (D15, D8, D9), input map (D3, D6, D7, D12, D16), release follows press | Done, host-tested; in the emulator on DT2 and DN2 |
| Macro math, per-track lists, frame-time apply held in each destination's range (W2) | Done; in the emulator on DT2 and DN2 ([s3](findings/s3-frame-values.md)) |
| Screens in the stock layout, firmware font; a macro with no parameter shows the MIDI pages' mark for a free knob (S4) | Done on DT2 and DN2 (the mark measured on DT2) ([s4](findings/s4-screens.md)); previews in `docs/img` use the mod's own font |
| Destinations with the firmware's names (S6, W4) | DT2: 28 per-track destinations, DN2: 26 ([s6](findings/s6-destinations.md)) |
| LEDs (W1) | Done on DT2 and DN2 ([s2](findings/s2-leds.md)) |
| Stored configuration (W3) | Done on DT2 and DN2 ([s5](findings/s5-storage.md)) |
| LEVEL/DATA and FUNC + NO (D16) | Done on DT2 and DN2 ([s1](findings/s1-input.md)) |
| Header title, naming macros, TRK + trig, firmware views over the mode, stock depth format | Done on DT2 and DN2 ([s1](findings/s1-input.md), [s4](findings/s4-screens.md)) |
| Setups per project or per kit, kept apart, saved in the project; copies take them along; PERSONALIZE > MACRO SETUP (D4); an optional feature; the knob positions are part of the setup (D14) | Done on DT2 and DN2, on core's kit and project data; the knob positions and the format with them on DT2, in the emulator ([s7](findings/s7-project-storage.md)) |
| Macro mode as a page (D6): the keys it doesn't use keep their stock function; menus, popups and mute mode come over the macro screen and the mode stays on; TRK + trig keeps the mode | Done on DT2 in the emulator (`tests/emu/scenario.py` `views_over`) and partly on a unit (the screen under mute mode and the pattern popup, TEMPO); DN2 not run ([s1](findings/s1-input.md), "Views over the mode") |

Still open:

| Where | Now | To do |
| --- | --- | --- |
| Machine-dependent source parameters (all of DN2's), LFO parameters | Not offered | S6 |
| Audio | The zero-DSP runner makes no sound | Coupled emulator or a unit |
| Storage claims, other mods | elekloader's linker refuses the same names; `tools/storage.py` also checks overlapping card ranges from manifests; core's guard at run time | elekloader to merge core-dt2 1.1 (#221; the submodule points at its branch) |
| The Digitone II | Its code and manifest are kept; not built or checked since core-dn2 0.3 left #221 (elekloader released another 0.3) | A Digitone II core with core-dt2 1.1's events and kit data, then `DEVICES` in `tools/common.sh` |
| Kit manager, PASTE KIT, undo | Not driven in the emulator (pattern copy and paste is) | Find their keys and add them to `tests/emu/project.py` (S7) |
| Views over the mode on the Digitone II | The same code (mute mode by its class name, the knobs by the firmware's handlers), not run there | Run `views_over` on DN2 (S1) |
