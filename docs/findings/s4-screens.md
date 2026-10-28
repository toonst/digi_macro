# S4: the screens (Digitakt II 1.17, Digitone II 1.12)

Spike S4 of [PLAN.md](../../PLAN.md): draw the macro and setup screens like the stock ones. Marks as digikit's
(**[D]** read or measured once here, **[O]** open, **[V]** checked by a second agent; nothing here is **[V]** yet).

## Text **[D]**

`0x40113dc8` is the firmware's printf-style text routine (digikit's `cf.text.fn.draw_fmt`): `(bitmap, font, x, y,
flags, fmt, ...)`. `y` is the text's bottom row counted from the bottom of the screen; flags bit 1 centres on `x`. The
stock views pass the font object `0x44d2e70c` (for example at `0x40105b16`); its capitals are 5 pixels high. On
the Digitone II 1.12 the routine is `0x4011573c` and the font `0x44508ef8` (the font both pass most, `0x18` above the
next, as on the Digitakt II). The glue
draws a string whose top row is `ytop` at `y = 59 − ytop`. Measured: the scenario's screens show the firmware's
glyphs where the host previews show the mod's own.

## Layouts, from the emulator's frames **[D]**

Rows counted from the top, columns from the left.

| Screen | What and where |
| --- | --- |
| Header (main screen and the parameter pages) | Rows 0–9: the pattern box, the name or the page's title, the tempo |
| Parameter page (SRC, FX, ...) | Page box `x 0–14, y 13–36`, corners cut; under it the pattern VOL fader `y 40–56`. Knobs of radius 8 centred at `x 33, 59, 85, 111`, `y 21` and `48`; labels with their top row at 32 and 59 |
| MODULATION SETUP (FUNC + FLTR, then a source) | Title at rows 0–4, the source's name at 6–10, both centred. Dotted separators at `x 17, 45, 73, 101`, rows 15–63, every other row. Column centres `x 31, 59, 87, 115`: the page heading (META for none) at row 25, the destination's short name at 33, the depth knob (radius 8) at `y 48`, the depth at 59. Left column: the source's icon at `x 10–14, y 28–32`, a fader box `x 3–13, y 40–55` with dotted sides, a base line at 56 (`x 1–15`), the value at 59 |

The macro screen keeps the header and draws the page box (MAC) and the knob grid without the VOL fader; the setup
screen draws the MODULATION SETUP layout with MACRO MODE as the source's name, the macro's letter for the icon and its
position on the fader. `src/mm_draw.c` has the coordinates.

## The destination list **[D]**

Turning a destination knob of the stock MODULATION SETUP opens a list in place of the columns (dumped from
AFTERTOUCH on the Digitakt II): a box `x 0–24` from row 13, open at the bottom, with `MOD1` / `DEST` and a wave in the
narrow font; one line every 7 rows from row 17, the page (META, SRC, …) at `x 28` in the narrow font on a page's
first line and the long name at `x 46`; a dotted line between pages; the selected line inverted, `x 45–122`; a
scroll bar at `x 124–127`. The choice applies while turning; NO closes the list. The mode draws the same, with the
macro's slot for MOD1.

## Fonts **[D]**

The narrow font (3-pixel letters, lowercase too) is a firmware font object: DT2 `0x44d2e6f4`, DN2 `0x44508ee0`, `0x18`
below the main one on both. The stock list, its box and the parameter pages' box use it; drawn with it, the mode's
list is pixel for pixel the stock AFTERTOUCH list.

## The header's message **[D]**

A page shows its title in the header as it opens ("Filter (1/2)"); `UIStates` (a singleton, DT2 `*0x44f370c0`, its
getter `0x401998c0`; DN2 `*0x446488c8`, `0x4018b2a4`) holds the message at `+72` (a std::string) and its time at
`+76`. `0x4005b23a(ui, fmt, ...)` (DN2 `0x400562e6`) sets it, as the stock does for "Ptn reloaded from …". The mode
shows "Macro Mode" when it is entered.

The firmware returns pointers in d0, where this compiler's m68k ABI reads a pointer result from a0: calling the getter
as returning a pointer gave a stale a0 (the view controller) and froze the UI. The glue calls routines that return a
pointer as returning a number.

## The naming screen **[D]**

`NameView` (vtable DT2 `0x4021d5ac`, DN2 `0x40206f78`) is the firmware's naming screen. A MIDI machine opens it to name
a parameter (FUNC + hold a knob) with its second constructor, DT2 `0x40117c70`, DN2 `0x401195e4`:
`(view, &title, &name, 0, 5, 0, &done)`, title "MIDI PARAM NAME", 5 characters, `done` a
`std::function<void(std::string)>` called with the new name on YES. The view is made as `std::make_shared` makes it
(`operator new` DT2 `0x4011ebd0`, 0xb0 bytes: the control block, vtable DT2 `0x401ef190` / DN2 `0x401e0fd8`, then the
view) and pushed with `0x4011c08a(Brain + 0x40, &shared_ptr, 0)` (DN2 `0x4011d9fe`). A std::string is one pointer to
its characters (`0x401e7e7a(&str, cstr, &alloc)` builds one, `0x401e63ca` frees it). The mode opens it the same way,
titled "MACRO NAME", with a std::function whose functor (the macro) is stored in place.

## Depths **[D]**

The stock modulation depths run −128.00 … 127.00 (turned to the ends on AFTERTOUCH), in 1/256 steps (a slow turn moved
0.03), shown with two decimals in six characters (`-128.0`). Their knobs turn through the encoder filter with the
configuration `0x44d2e670` (DN2 `0x44508e5c`), the count x12 when pushed (whole steps then). The mode's depths are
the same.

## Open **[O]**

- How the stock setup screen marks the selected column (the mod inverts its heading, rows 19–35).
- The Digitone II's stock pages use the same header and page box (seen in the emulator); its MODULATION SETUP was
  not dumped.
