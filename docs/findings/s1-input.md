# S1: input (Digitakt II 1.17, Digitone II 1.12)

Spike S1 of [PLAN.md](../../PLAN.md): which events the mode gets and when it may start. Marks as digikit's (**[D]**
read or measured once here, **[O]** open, **[V]** checked by a second agent; nothing here is **[V]** yet).

## Events **[D]**

Measured with `tests/emu/scenario.py` (the zero-DSP runner, an elekloader build of core-dt2 1.0 and this mod; the
same on the Digitone II with core-dn2 0.2):

| Input | What arrives |
| --- | --- |
| Encoders A–H | `ev_enc` ids 1–8 |
| LEVEL/DATA | `ev_enc` id 9 (the emulator's panel encoder 9); pushed, key code 49 |
| Pushes A–H | `ev_key` codes 41–48 |
| FUNC + NO | `ev_key` NO with FUNC held; taking the press keeps the stock reload from running: the macros go to the centre and the screen stays the macro screen |

## The encoder filter **[D]**

Every stock knob turns through one filter (digikit finding 03 has it on the Digitone II 1.11 at `0x4011336e`): after
the knob rests it swallows the first counts, then moves more per count the faster it turns. Found here by its
constants (a per-knob counter refilled to 48, a threshold of 24, `3 |count|` taken off, an `lsl #8`):

| | DT2 1.17 | DN2 1.12 |
| --- | --- | --- |
| `filter(block, knob, count, cfg, pushed)` | `0x40111cda` (61 callers) | `0x4011364e` (71) |
| `list_filter(block, event, cfg)`: knob, count x8 when pushed, then `filter` | `0x40111e42` | `0x401137b6` |
| The block's constructor (252 bytes; it starts a timer that refills the counters) | `0x40111b68` | `0x401134dc` |
| The configuration of the stock 0–127 parameters (LEV, VOL, the sends, amp): raw units of 256 a step, the count x2 when pushed | `0x44d2e654` | `0x44508e40` |
| The configuration the MODULATION SETUP passes for its destination knobs: steps | `0x44d2e600` | `0x44508dec` |

An encoder event has the knob at `+12`, the count at `+16` and a "pushed" byte at `+20` (helpers `0x40111a4e`,
`0x40111aaa`, `0x40111ac8`). A parameter's configuration is the word at `+16` of its run-time descriptor,
`0x4293c040 + 0x44 id` on the Digitakt II (`0x400c892e(id)`): LEV, VOL, the sends and the amp envelope have
`0x44d2e654`, FREQ and TUNE `0x44d2e670`.

The mode builds one block with the constructor and turns its macro, depth and LEVEL knobs through `filter` with the
parameters' configuration, its destination knobs through `list_filter` with the setup's. Measured: the same 38 turns
(1 to 32 counts) move the stock LEV knob (DT2) or Delay Send (DN2) and macro A by exactly the same steps.

## Held keys **[D]**

The firmware does not always pass a key's release on: after FUNC + NO (the pattern reload) the FUNC release never
reaches `ev_key`, so a "FUNC held" flag kept from presses and releases stays set. The mode reads held keys from the
firmware's key-down query instead, `int key_down(code)` (DT2 `0x4011e150`, DN2 `0x4011fac4`; digikit finding 58): FUNC,
the pushes and the entry chord's first key.

## Menus and dialogs (D9) **[D]**

The view controller sits at Brain `+0x40`; its view stack is a list whose sentinel is at `+0x14` (the sentinel's
second word is the top node, the view at node `+8`, its vtable at the view's first word). On the main screen and the
parameter pages the top view's vtable is `HelpBubbleView`'s (DT2 `0x40216f14`, DN2 `0x402008e8`, found by its
typeinfo name `14HelpBubbleView`); a menu, dialog or popup pushes another view on top. The glue
(`fw_can_enter`) lets PAGE + MOD start the mode only with that view on top; the Brain offsets are the same on both.

In the scenario, with SETTINGS open the chord leaves the mode off. The MOD press itself still goes to the firmware,
which closes the menu and shows its MOD page, as without the mod.

The boot's file-system windows (VERIFYING, FILE SYSTEM OK) are such views too: the scenario closes them with NO until
that view is on top.

## Open **[O]**

