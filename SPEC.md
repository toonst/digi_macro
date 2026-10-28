# Requirements for a Digitakt II and Digitone II performance macro firmware mod

A new mode for the Digitakt II and the Digitone II.

Names are the panel's: the MOD key (the MOD page key) and the PAGE key, FUNC (these devices have no SHIFT key),
encoders A–H and their pushes, and the trig keys 1–16.

Pressing MOD + PAGE enters macro mode: each of the 8 encoders (A B C D E F G H) becomes a macro that controls 4
parameters, assignable across tracks.

The MOD key lights up blue while macro mode is enabled.

Pressing the MOD key on its own exits macro mode.

The display shows the state of 8 round knobs, default state in the middle.

Open Question:
Implement as separate mode or as a macro machine?
Macro machine: one track is dedicated to the macro.
Maybe sequencing of the macro is possible (complex, but doable).

Separate mode: maybe simpler in implementation, no dedicated track needed,
no parameter locks of the macros.

For now we choose to implement a separate mode.

## Separate mode

Once in macro mode, select a macro to configure with FUNC + pushing its encoder (A–H).

FUNC + push immediately opens the configuration screen for that macro,
which looks a lot like the aftertouch, key tracking, etc. modulation setup.

Inside this modulation setup screen, always one of the four parameters is selected.
The selected parameter is visible by an inverted graphic on the display.

Different from the key tracking, aftertouch, etc. setup, the trig keys show for the selected parameter to which
tracks (1–16) it applies. The trig keys of the selected tracks light up red, similar to the CONTROL ALL configuration
and the compressor routing, but without showing their screens.

Pressing NO exits the modulation setup for that macro and goes back to macro mode.
