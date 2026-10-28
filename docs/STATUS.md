# Status

Against [PLAN.md](../PLAN.md) section 7. Measured on the Digitakt II 1.17 and the Digitone II 1.12 in digikit's
emulator with `tests/emu/scenario.py` (32 checks each, all passing); host tests with `tools/check.sh`.

| Part | State |
| --- | --- |
| Mode logic: entry and exit (D15, D8, D9), input map (D3, D6, D7, D12, D16), release follows press | Done, host-tested; in the emulator on DT2 and DN2 |
| Macro math, per-track lists, frame-time apply held in each destination's range (W2) | Done; in the emulator on DT2 and DN2 ([s3](findings/s3-frame-values.md)) |
| Screens in the stock layout, firmware font (S4) | Done on DT2 and DN2 ([s4](findings/s4-screens.md)); previews in `docs/img` use the mod's own font |
| Destinations with the firmware's names (S6, W4) | DT2: 28 per-track destinations, DN2: 26 ([s6](findings/s6-destinations.md)) |
| LEDs (W1) | Done on DT2 and DN2 ([s2](findings/s2-leds.md)) |
| Stored configuration (W3) | Done on DT2 and DN2 ([s5](findings/s5-storage.md)) |
| LEVEL/DATA and FUNC + NO (D16) | Done on DT2 and DN2 ([s1](findings/s1-input.md)) |

Still open:

| Where | Now | To do |
| --- | --- | --- |
| `MM_COARSE` | Push + turn moves 8 steps | The stock step (S1) |
| `MM_LED_*` | Blue `0x41`, red `0x11`, off `0x01` | Colours on a unit (S2) |
| Machine-dependent source parameters (all of DN2's), LFO parameters | Not offered | S6 |
| Audio | The zero-DSP runner makes no sound | Coupled emulator or a unit |
