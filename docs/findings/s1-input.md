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

## Key event flags **[D]**

From the mode's key log (`mm_keylog`, read by the scenario): bit 0 is "down". A press has `0x01` (`0x03` with FUNC
held), a held key repeats with `0x09` (`0x0b`), and a release has bit 0 clear: `0x10` for most keys, but FUNC's and
PAGE's come with no bit set at all (`0x00`). The mode first treated only `0x10` as a release, so FUNC looked held for
good after any FUNC chord: a push alone opened the setup, and NO there reset the macros (the round-1 report).

## Views FUNC and TRK put on **[D]**

While FUNC is held the firmware puts `QuickMuteMenuView` on top (DT2 `0x401f9208`, DN2 `0x401e1eb4`), while TRK is
held `TrackSelectionView` (DT2 `0x401f9ec8`, DN2 `0x401e2b68`).

## Views over the mode **[D]**

Macro mode is a page (PLAN.md D6), so a view the firmware puts over a page comes over the macro screen the same
way. The page's views end with the one on top on the main screen and the parameter pages (`HelpBubbleView`, below);
every view above it in the view controller's list is over the page: a menu (PRESET, SETTINGS, SAMPLES, TEMPO), a
popup (pattern, bank, song), mute mode, `TrackSelectionView`, the naming screen. `QuickMuteMenuView` is not counted:
the mode keeps the keys under it (FUNC + push, FUNC + NO) and draws over it.

The classes, by their RTTI on DT2 1.17 (each vtable's word before slot 0 is its typeinfo, whose `+4` is the mangled
name; the method gives back the three vtables found before, and NameView's):

| Class | Vtable (DT2 1.17) | EncoderHandler slot 2 | What it is |
| --- | --- | --- | --- |
| `HelpBubbleView` | `0x40216f14` | default | the top of the page's views (VIEW_TOP_BASE) |
| `QuickMuteMenuView` : `MuteMenuView` | `0x401f9208` | default | FUNC held (VIEW_FUNC) |
| `PermanentMuteMenuView` : `MuteMenuView` | `0x401f8a18` | default | mute mode, FUNC + TRK (on top in the emulator) |
| `TrackSelectionView` | `0x401f9ec8` | default | TRK held |
| `PatternSelectView`, `BankSelectMenuView` | `0x40217300`, `0x4021954c` | default | PTN, FUNC + PTN |
| `TapTempoView`, `PopupWindow` | `0x4021beec`, `0x4021d8c8` | default | |
| `TempoMenuView` | `0x40207874` | `0x400bac66` | TEMPO (opaque, full screen) |
| `NameView` | `0x4021d5ac` | `0x40117934` | the naming screen |
| `SongModePopup`, `SongEditView`, `PatternChainView` | `0x401fc34c`, `0x401fbe14`, `0x40217168` | own | SONG, song edit, chains |

- **The firmware's knob dispatch.** `Brain::enc` (`0x40030b64`) resets the screen saver (`0x40030b50`), copies the
  `EncoderEvent` (vtable `0x401f10c4`, the same class the UI loop built with `0x401119b8`, which the mod's `ev_enc`
  gets) and calls the dispatch `0x4011c674`. That walks the view controller's list from the top (`0x4011c41a`, under
  its lock) and asks each view's EncoderHandler (the base at View `+4`, its vtable slot 2: `0x4011b5da` calls
  `(view + 4)->vtbl[2](view + 4, event)`, a bool); the first that takes the turn ends the walk, and one whose
  closing byte (`+0x30`) is set asks for the sweep (controller `+0x22`). The default handler, `0x40111f30`, is
  `clr.b d0; rts`: a view that keeps it passes every turn on, down to the page.
- **Knobs.** So a view like mute mode or the pattern popup leaves every knob to the page under it, and the macros
  are that page: the glue asks the views over the page the same way (`over_takes_enc`), and a turn none of them
  takes is the mode's, LEVEL/DATA too. TEMPO, the naming screen and the song screens take what their handlers take.
  Before, with the knobs left to the firmware under any view, knob A in mute mode turned the LFO page's SPD behind
  the macro screen (seen on a unit).
- **Keys.** A view over the page gets every key (the glue's `covered`). The mode still sees them: a page key, or
  FUNC + MOD, leaves it (the firmware closes the view and opens the page); MOD goes to the firmware (it closes a
  menu) and the mode stays; a release follows its press.
- **Mute mode** (`PermanentMuteMenuView`, known by its name, so on the Digitone II too) leaves the mode its keys:
  the trig keys mute and FUNC + trig prepares mutes (they pass to the firmware, as on the macro screen they always
  do), but the pushes, FUNC + push (the setup, the naming screen), MOD (leave) and PAGE + MOD (enter, D9) are the
  mode's.
- **Screen.** `ev_draw` runs after the firmware composed the frame, so to have the views over the page drawn over
  the macro screen it composes the frame again: it clears it, paints the page's views from the topmost opaque one up
  (View vtable slot 4, `paint(view, bitmap)`, as the firmware's render does and digikit's core/ui overrides it),
  draws the macro screen, then paints the views over the page. If one of those is opaque (View `+0x16`; views below
  the topmost opaque one are not drawn: TEMPO, the menus) it hides the page, and the firmware's frame stays.
- **LEDs.** Under a view that takes the keys the setup screen's track LEDs are left to the firmware (the pattern
  popup's patterns); MOD stays blue.

Measured in the emulator (DT2 1.17, `tests/emu/scenario.py` `views_over`): PRESET, SETTINGS, TEMPO and PTN open over
the mode and close back to it; knob A moves macro A under the pattern popup and in mute mode; in mute mode
LEVEL/DATA moves and centres the macros, FUNC + push opens the setup, MOD leaves (mute mode stays) and PAGE + MOD
enters; KEYBOARD keeps the mode; FLTR from a menu leaves it. On a unit: the macro screen stays under mute mode and the
pattern popup, TEMPO works. Open: the Digitone II, where none of this has run; it needs no new address there (the
class name, the View layout and the handler slot).

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

