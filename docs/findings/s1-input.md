# S1: input (Digitakt II 1.17)

Spike S1 of [PLAN.md](../../PLAN.md): which events the mode gets and when it may start. Marks as digikit's (**[D]**
read or measured once here, **[O]** open, **[V]** checked by a second agent; nothing here is **[V]** yet).

## Events **[D]**

Measured with `tests/emu/scenario.py` (the zero-DSP runner, an elekloader build of core-dt2 1.0 and this mod):

| Input | What arrives |
| --- | --- |
| Encoders A–H | `ev_enc` ids 1–8 |
| LEVEL/DATA | `ev_enc` id 9 (the emulator's panel encoder 9): turning it by +6 and −6 moves macro C from −6 to the centre and back |
| Pushes A–H | `ev_key` codes 41–48 |
| FUNC + NO | `ev_key` NO with FUNC held; taking the press keeps the stock reload from running: the macros go to the centre and the screen stays the macro screen |

## Menus and dialogs (D9) **[D]**

The view controller sits at Brain `+0x40`; its view stack is a list whose sentinel is at `+0x14` (the sentinel's
second word is the top node, the view at node `+8`, its vtable at the view's first word). On the main screen and the
parameter pages the top view's vtable is `0x40216f14`; a menu, dialog or popup pushes another view on top. The glue
(`fw_can_enter`) lets PAGE + MOD start the mode only with `0x40216f14` on top.

In the scenario, with SETTINGS open the chord leaves the mode off. The MOD press itself still goes to the firmware,
which closes the menu and shows its MOD page, as without the mod.

The boot's file-system windows (VERIFYING, FILE SYSTEM OK) are such views too: the scenario closes them with NO until
`0x40216f14` is on top.

## Open **[O]**

- The stock step of push + turn (`MM_COARSE` is 8 for now).
- The same on the Digitone II 1.12.
