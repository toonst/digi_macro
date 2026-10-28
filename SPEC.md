
# Requirements for Digitakt 2 and Digitone 2 performance macro fw mod

New mode for the Digi's:

MOD + PAGE press enters macro mode: each of the 8 turning knobs ( A B C D E F G H) become a 4 parameter, across track assignable macro.

The MOD knob lights up blue while macro mode is enabled.

Pressing just the MOD knob exits the macro mode.

The display shows the state of 8 round knobs, default state in the middle.

Open Question:
Implement as separate mode or as a macro machine?
Macro machine: one track is dedicated to the macro.
Maybe sequencing of the macro is possible (complex, but doable).

Separate mode: maybe simpler in implementation, no dedicated track needed,
no param locks of macro buttons.

For now we choose to implement a separate mode.

## Separate mode

Once in macro mode, select a macro to configure by shift + pressing the (A-H) knob.

Shift + press immediately leads you to the configuration screen fr that knob,
which looks a lot like the after touch, key tracking etc modulation setup.

Inside this modulation setup screen, always one of the four parameters is selected.
The selected parameter is visible by an inverted graphic on the display.

Different than the key tracking/after touch etc setup, the main keys indicate for the selected parameter to which tracks (1 - 16) it applies.
Selected tracks for that parameter light up red, similar to control all config and compressor routing, without the display of control all config or compressor routing.

Pressing NO exists the macro modulation setup for that macro knob, going back to
macro mode.





