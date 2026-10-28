# S4: the screens (Digitakt II 1.17)

Spike S4 of [PLAN.md](../../PLAN.md): draw the macro and setup screens like the stock ones. Marks as digikit's
(**[D]** read or measured once here, **[O]** open, **[V]** checked by a second agent; nothing here is **[V]** yet).

## Text **[D]**

`0x40113dc8` is the firmware's printf-style text routine (digikit's `cf.text.fn.draw_fmt`): `(bitmap, font, x, y,
flags, fmt, ...)`. `y` is the text's bottom row counted from the bottom of the screen; flags bit 1 centres on `x`. The
stock views pass the font object `0x44d2e70c` (for example at `0x40105b16`); its capitals are 5 pixels high. The glue
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

## Open **[O]**

- How the stock setup screen marks the selected column (the mod inverts its heading).
- The same on the Digitone II 1.12 (text routine `0x4011573c` there, per digikit's port file; its font object is not
  read yet).
