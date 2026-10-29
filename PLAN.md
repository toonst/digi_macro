# Macro mode: implementation plan

Status: plan, revision 2, 2026-10-28. Requirements: [SPEC.md](SPEC.md). Revision 1 built everything inside digikit;
this revision builds on elekloader instead, after reading three projects:

| Project | Commit read | Role in this plan |
| --- | --- | --- |
| [elekloader/elekloader][el] | `a401967` | **Platform.** The mod format, the hook bus on both devices, the linker, the build and the distribution |
| [toonst/digikit][dk] | `5e62a52` | **Lab.** The emulator for DT2 1.17 and DN2 1.12, the firmware databases and tools, and the findings that locate most of what we need |
| [irpina/digiemu][de] | `f57c2f2` | **Not used.** It emulates the mk1 devices; one lead on LED colours (section 2.2) |

Targets: **Digitakt II OS 1.17** and **Digitone II OS 1.12**, the only Digitakt II and Digitone II releases that
elekloader's cores and digikit support. Evidence marks follow digikit: **[V]** checked by a second agent against
image bytes, **[D]** read or measured once, **[O]** open. Addresses from digikit findings for DT2 1.16 or DN2 1.11
are references only: they must be found again on 1.17 / 1.12 before use.

## 1. Summary

Macro mode is built as an **elekloader mod** (format 2, linked onto `core-dt2` and `core-dn2`), with **digikit as
the lab**.

elekloader already has the input and screen half of macro mode on both devices:

- `ev_key` and `ev_enc` hand every key and encoder event to the mods before the firmware sees it, and a mod can take
  the event.
- `ev_draw` lets a mod draw over every composed frame; `ev_tick` lets it ask for a redraw.
- `ev_settings` and `ev_personalize` add menu rows.

In revision 1 these were the input filter and the module-owned views, the riskiest new work. On hardware,
`ev_key` and the PERSONALIZE row are confirmed on a Digitakt II, and all six UI events on a Digitone II.

For the other half, elekloader has proven designs on other devices that we port to the Digitakt II and Digitone II:

| Need | elekloader's design, on another device |
| --- | --- |
| LED overrides | `ev_key_leds` (Digitone mk1): mods set key LEDs before the stock views; the first setter of an LED wins |
| Non-destructive macros (D1) | `ev_render_params` (Digitone mk1): each block's parameter words, before the stage where the mod wheel, aftertouch and the LFOs ride on them. "A change lasts this block … the kit and the targets are untouched" |
| Storage, per pattern or project later (D4) | Mod storage (Analog Rytm MKII and Analog Four MKII): fields per sound, kit, global, track, pattern or project, in bytes the stock OS never reads, so projects still load on the stock OS |

digikit supplies what elekloader lacks on these devices: the only emulator that runs DT2 1.17 and DN2 1.12
(ColdFire and SHARC; elekloader checks its own Digitakt II and Digitone II cores in it), a control protocol to press
keys and read the screen and LEDs, hash anchors and firmware databases to find addresses on both images, and
findings that already locate the LED path, the modulation code, the parameter tables and the drawing routines.
**No digikit change is needed.**

**Payoff and confidence.** The payoff is the spec's macro mode on both devices, built on hooks that already run on
units, with a first demo in the first week (M1). It can be shared through elekloader's builder.

Estimated effort: 26–42 working days. That is about the same total as revision 1, but the work is different. The
view work disappears; new work appears for storage and for finding every address once per device by hand.

Confidence: high (about 85 %) for the UI half. Medium (about 60 %) for non-destructive application until spike S3
has run. D1 is fixed, so writing the kit is no longer a fallback: **stop and re-plan** if S3 finds no hook point.

## 2. What the three projects offer

### 2.1 elekloader

- **What it is.** A mod loader. It builds a custom OS from the user's own stock file, and mods carry only their own
  bytes. Format-2 mods are relocatable objects that the linker places on top of one core mod per device (the hook
  bus). The linker refuses overlapping bytes, overlapping memory and duplicate claimed names.
- **Building and licence.** It seals Digitakt II and Digitone II images with their HMAC, and builds in Python, in the
  browser (Pyodide) and in desktop apps. Licence GPL-2.0-or-later.
- **Status on the targets.** The Digitakt II core `core-dt2` 1.0 (OS 1.17) is not released. The Digitone II core
  `core-dn2` 0.2 (OS 1.12) is experimental and not released.
- **The hook sites** ([core-dt2][el-dt2], [core-dn2][el-dn2]):

  | Event | DT2 1.17 site | DN2 1.12 site | Contract ([ADAPTING.md][el-adapt], "The hook bus") |
  | --- | --- | --- | --- |
  | `ev_tick` | `0x40032ad4` | `0x4002e464` | `void f(ctrl)`, UI task, about 30 Hz (measured on mk1; not on mk2). Set `ctrl+0x20 = 1` to redraw |
  | `ev_draw` | `0x40032b3a` | `0x4002e4ca` | `void f(bmp, ctrl)` after `drawAll`; draws over the frame. `y = 0` is the bottom row |
  | `ev_key` | `0x40033d94` | `0x4002f20c` | `int f(brain, ev)`: `ev+12` key code, `ev+16` flags (1 pressed, 8 held about 0.4 s, 2 FUNC held, 4 double press, 0x10 released). Nonzero takes it: nothing later sees it, the stock included |
  | `ev_enc` | `0x40033dde` | `0x4002f256` | `int f(brain, ev)`: `ev+12` encoder 1–8 (A–H), `ev+16` delta; nonzero takes it |
  | `ev_settings`, `ev_personalize` | `0x400a5eac`, `0x4009e2a2` | `0x4009ff88`, `0x400987a2` | `void f(menu)`; `core_additem(menu, row)` adds a row |

- **Mods' memory.** DDR `0x47F00000–0x47F40000` (256 KB, shared by all mods) on both devices. The Digitakt II also
  has `.fast` SRAM, but it is copied in on the first tick, so code that may run before that must be `.run`.
- **Rules that matter here.** Mods call firmware routines by address (one manifest per device and OS). They claim
  named resources such as key combinations (`ui:...`). They do not patch core's sites. A missing event is built
  "in the core's shape, in a file of its own" and offered to core by pull request; the project asks for these.
- **Closest examples.** [perform-direct][el-pd] (DT2: flips the FUNC bit on PRESET's key events in `ev_key`, so
  PRESET alone toggles PERFORM; confirmed on a unit), [hooks-check-dn2][el-hc] (every UI event, taking and passing
  keys; confirmed on a unit), hello-marker-dt2 / -dn2 (`fillRect` at DT2 `0x401131ae`, DN2 `0x40114b22`).
- **Designs to port** ([ADAPTING.md][el-adapt]): "Key LEDs and each block's sound parameters" (core-dn1 3.3), "Mod
  storage (core-ar2 0.2)", and "The Mod Menu" (core-dn1 2.2: a mod-owned full screen opened by a key gesture).
- **Prior art in the shop.** *digimatrix* (gdeo607/digi1_mods, Digitakt mk1, MIT): "any track's LFO to any parameter
  of any track, in 8 slots, per pattern". It is for another device and lives in another repository, so it was not
  read here; its UI and per-pattern storage may be worth a look.
- **Name clash.** The shop already has *Digi Macro* (a Plaits machine for the Digitakt mk1). This mod needs another
  id and title (D13).

### 2.2 digiemu

- An emulator for the Digitakt mk1, Digitone mk1, Model:Cycles and Model:Samples, with a firmware check
  (`emu.fwcheck`) for custom builds. It started from an early digikit. Its Digitakt II and Digitone II code stays
  "as upstream left them", from the 1.15C/1.16 era. It does not run 1.17 or 1.12, so it cannot test this mod.
- **One lead.** On the mk1, the panel MCU's firmware is embedded in the main OS image (`0x4024E94C` on DT mk1 1.53).
  digiemu decoded the LED protocol (palette entries, four slots per LED, selectors) from both ends
  ([emu/panelleds.py][de-led]). digikit only infers the meaning of the Digitakt II's `b9 idx val` records (finding
  19, **[O]**). Spike S2 looks for the same embedded firmware in the 1.17 / 1.12 images, to learn which value is
  blue.

### 2.3 digikit

From revision 1. These are the facts this plan uses:

- **Panel.** Key codes, the LED index per key and the LED value byte: `devices/*-leds.toml` and [finding 19][f19]
  (setter `LedManager(logical, layer, value)`: "a nonzero layer blocks the next set").
- **Modulation.** It runs on the ColdFire per frame, before the DSP frame is built. The kernel applies one source
  through four destinations `depth << 16 | dest` and saturates. The Digitone II driver runs six sources × 4
  destinations × 16 tracks. The LFO tick runs once per frame. All of this is from [finding 04][f04], at 1.16 / 1.11
  addresses.
- **Parameters.** The descriptor table (codes, value-array indices, ranges, names; [finding 02][f02]). The value
  array per track: DT2 71 entries, DN2 101; the modulation bounds are 70 / 100. Raw values run 0 … 0x7f00.
- **Drawing on 1.17.** Printf text DT2 `0x40113dc8` / DN2 `0x4011573c`, `fill_rect`, invert, blit, text width, and
  the font pointer (DT2 `0x402a9be0`) ([finding 71][f71] section 3). The stock destination list is `ModDestListView`.
- **Keys.** Key event flags, and FUNC + MOD opening the Master compressor ([finding 03][f03]).
- **Kit records.** Spare bytes in the kit's FX storage record on DT2 1.17 ([finding 61][f61]): a lead for storage.

digikit's latest handover asks that an external loader project not be cited in the digikit repository. If anything
from this work goes back to digikit (a finding, a tool fix), keep elekloader out of it.

## 3. Decisions

All decided by the user:

| # | Decision |
| --- | --- |
| D1 | Macros apply **on top**, like the other modulations (aftertouch, key tracking, mod wheel): the kit is never changed |
| D2 | Macros **keep their state** when the mode is left |
| D3 | **Push + turn** changes a macro in bigger increments, and **push + NO** resets it to the centre, as for the stock parameters |
| D4 | The configuration is **global** in the first version. Then per kit or per project, chosen by a PERSONALIZE row (done) |
| D5 | Depth per slot as the stock modulation setups: −128.00 … +127.99 (1/256 steps), 0 = off; at full depth and full turn the offset spans the whole parameter range. Macro positions stay whole steps, −64 … +63, as the stock 0–127 parameters |
| D6 | Outside the setup screen the **trig keys keep their usual function**, in whatever mode they are in (in mute mode they stay mute keys). PLAY, STOP, RECORD, PTN, SONG, TRK and FUNC keep theirs too; they act behind the macro screen. The LEVEL/DATA encoder, its push and FUNC + NO are the mode's (D16); the page keys leave the mode and open their page. Only the setup screen gives the trig keys another function (4.2) |
| D7 | The setup screen works like the stock modulation setups: **A** = slot 1 destination, **E** = slot 1 depth, **B / F** = slot 2, **C / G** = slot 3, **D / H** = slot 4 |
| D8 | On the setup screen **MOD goes back** to the macro screen, as NO does; a second MOD press, on the macro screen, exits macro mode |
| D9 | Macro mode can be entered from the main screen and the parameter pages (no menu or dialog open) |
| D10 | MIDI tracks are never targets (not planned) |
| D11 | Destinations are the per-track parameters the stock modulation destination list offers (SRC, FLTR, AMP, track FX sends, LFO); kit-wide FX later |
| D12 | On the setup screen the selected slot is the one whose knob (A–D or E–H) was last turned or pushed; LEFT / RIGHT also step it |
| D13 | Mod id `macro-mode`, title "MACRO MODE" (not "Digi Macro", which the shop has) |
| D14 | Macro positions are not stored: every boot starts at the centre |
| D15 | **Entry and exit act on presses**, never on releases: PAGE + MOD in either order enters, on the second key's press; PAGE held + MOD also exits, from the macro screen only; from the setup screen, leaving takes two MOD presses (4.1) |
| D16 | **LEVEL/DATA** moves all eight macros by the same amount; **FUNC + NO** (the stock reload) puts all eight back at the centre and does not reload the pattern, on either screen. On the setup screen LEVEL/DATA moves only the macro being edited |

## 4. Behaviour

### 4.1 States

```text
   PAGE + MOD (either order)                     FUNC + push A..H
  OFF  ----------------->  MACRO  --------------------------------------->  SETUP(k)
   ^                        |   ^                                             |
   |   MOD, or PAGE + MOD   |   |   NO or MOD (D8), also with PAGE held       |
   +------------------------+   +---------------------------------------------+
```

- **Entry, on the second key's press (D15).** Both orders enter, and the mode starts at once on the press that
  completes the chord, which the mod takes:
  - PAGE held, then MOD: the PAGE press goes to the firmware as usual (the mod cannot yet know MOD will follow);
    the MOD press is taken, so the LFO page does not open.
  - MOD held, then PAGE: the MOD press goes to the firmware, which opens the LFO page behind the macro screen; the
    PAGE press is taken.

  A MOD press the mod took itself (an exit, below) never counts as the first key of a chord, so pressing PAGE while
  still holding it does not enter again. FUNC + MOD is the stock Master compressor page; neither order of PAGE +
  MOD has a documented stock use, and S1 checks this.
- **Exit, on MOD's press.** On the macro screen a MOD press exits. On the setup screen it goes back to the macro
  screen (D8), with or without PAGE held: PAGE held + MOD exits from the macro screen only. So from the setup
  screen, leaving macro mode takes two MOD presses. With MOD held first, MOD's own press has already acted
  (exit, or back from the setup screen), and a PAGE press after it makes no chord: after an exit it goes to the
  firmware as usual, and on the macro screen it is taken.
- **Releases follow their press.** The mod remembers which presses it took and also takes their repeats and
  releases; a key whose press went to the firmware (the first key of the entry chord) has its release go there
  too. So the firmware's own record of held keys stays right.
- **Claims** (elekloader `resources.names`): `ui:PAGE + MOD`, `ui:macro mode` (the keys it takes while
  active).

### 4.2 Input map

| Input | MACRO | SETUP(k) |
| --- | --- | --- |
| Turn A–H | Move macro A–H: −64 … +63, through the firmware's encoder filter with the stock 0–127 parameters' configuration | A–D: destination of slot 1–4, in a list as the stock modulation setups show it (the choice applies while turning); E–H: depth of slot 1–4 (D7). The turned slot becomes the selected one (D12) |
| Push A–H held + turn | Bigger steps, as the stock knobs (the filter's pushed mode) | The same, on that knob |
| Push A–H held + NO | Reset that macro to the centre | Reset that knob: destination to none, depth to 0 |
| Push A–H alone | Taken (for push + turn, push + NO); it never opens SETUP | Selects that slot |
| FUNC + push A–H, let go | Open SETUP for that macro | Switch to that macro's setup |
| FUNC + push A–H, held (about 0.5 s) | Name that macro: the firmware's naming screen, as a MIDI machine's parameter (5 characters); the name labels its knob | The same |
| TRK + trig | Leave; the firmware selects that track on the page underneath | The same |
| FUNC + MOD | Leave; the firmware opens its FUNC + MOD page (compressor, mixer), as from any page | The same |
| LEFT / RIGHT | Pass | Step the selected slot |
| Trig 1–16 | Pass: their usual function, in their current mode (D6) | Toggle that track in the selected slot's mask |
| NO alone | Pass | Close the destination list; else back to MACRO (the configuration is saved) |
| FUNC + NO | Every macro to the centre; the pattern is not reloaded (D16) | The same, staying on SETUP |
| Turn LEVEL/DATA | Every macro moves by the turn (D16) | The edited macro moves (its fader) |
| Push LEVEL/DATA | Every macro to the centre | The edited macro to the centre |
| TRIG, SRC, FLTR, AMP, FX | Leave, and pass on: the firmware opens that page | The same (the configuration is saved) |
| MOD | Exit, on the press (4.1) | Back to MACRO, on the press (D8) |
| PAGE held + MOD | Exit | Back to MACRO, as MOD alone |
| PAGE | Taken (held for the exit chord) | Taken (held for the exit chord) |
| PLAY, STOP, RECORD, PTN, SONG, TRK, FUNC | Pass | Pass |
| Any other key or encoder | Taken | Taken |

`ev_key` runs at a low `order` (before other mods) and `ev_enc` likewise. Both stay `.run` code on the Digitakt II
(a key can come before the first tick).

### 4.3 Data

```c
typedef struct { uint8_t dest; int8_t depth; uint16_t tracks; } mm_slot;  /* dest = value-array index, 0 = none */
typedef struct { int8_t pos; mm_slot slot[4]; } mm_macro;                 /* pos -64..+63, 0 = centre */
static mm_macro mm_macros[8];                                            /* about 140 bytes */
```

Each slot has its own track mask: "for the selected parameter, to which tracks it applies".

### 4.4 Screens (128 × 64, drawn in `ev_draw` after the stock view)

Both copy the stock Digitakt II 1.17 screens ([s4-screens](docs/findings/s4-screens.md)); previews in `docs/img`.

- **Macro screen**, like a parameter page: the stock header stays (pattern, name, tempo; rows 0–9). Below it, the
  page box on the left reads MAC, with no pattern VOL fader under it, and the eight knobs sit in the parameter pages'
  grid (2 × 4, radius 8). Pointer up is the centre; the travel is ±135°. Under each knob is its letter, or the short
  name of its first slot's destination once one is set; while a knob turns, its value.
- **Setup screen**, like the stock MODULATION SETUP (FUNC + FLTR, then a modulation source): the title MODULATION
  SETUP and MACRO MODE. The left column is the source: the macro's letter and its position as a fader with the value
  under it. Then one column per slot, between dotted separators: the destination's page (SRC, FLTR, AMP, FX, PORT;
  META for none, as the stock) and short name (knob A–D), the depth knob (E–H) and the depth. The selected slot's
  heading is inverted.
- **Drawing.** The firmware's `fillRect` and printf text routine with its 5-pixel font; the knob raster is the mod's
  own (integer sine table). `ev_tick` sets the redraw flag only when the state changed.

### 4.5 LEDs

- In MACRO and SETUP the MOD LED is blue, and the other page keys (TRIG … FX) are dark: macro mode is the page.
- In SETUP the trig LEDs are red for the selected slot's tracks, and off for the others. The sequencer's playhead
  does not show there.
- In MACRO the trig LEDs stay the firmware's.
- The handler sets these LEDs from the new `ev_key_leds` (W1) only while the mode is active. With the mode off, it
  sets nothing and the firmware's LEDs return by themselves.

### 4.6 Applying a macro (D1)

For a macro at position `p` and a slot with depth `d`, on every track in the slot's mask:

```text
delta = (p * d * 0x7f00) >> 12               /* |p*d| <= 4096, so delta spans the parameter range */
value = sat(value + delta, 0, 0x7f00)        /* in the per-frame value array, as the stock modulators do */
```

- On the UI tick the mod turns its state into per-track `(dest, delta)` lists, merging slots that hit the same
  destination, and publishes them with one pointer swap.
- The per-frame handler (W2, interrupt level) only adds and saturates: at most one add per active
  (track, destination) pair, and nothing when every macro is at the centre.
- The kit, the sounds and what the parameter pages show stay as they are, as with an LFO.
- Destinations are value-array indices. The same index means different machine parameters on tracks with different
  machines, so the setup screen names a destination by the machine of the first track in the mask (or the active
  track), as the stock list does (W4).

### 4.7 Persistence (D4)

- **Version 1.** The configuration (8 × 4 slots: destination, depth, mask) goes into a global store (W3) in a
  versioned layout, about 120–140 bytes. It is written when the setup screen closes. Positions are not stored (D14).
- **Per kit or per project** (done; docs/findings/s7-project-storage.md). A PERSONALIZE row from `core_additem`,
  MACRO SETUP: PROJECT or KIT, chooses which setup is on. A setup (slots and names, not positions) is part of a kit,
  and the project has its own; both are kept whichever is on. They go into bytes of the saved project the stock OS
  leaves unused (each kit record's tail, the header), the same way on both devices, and the firmware's copies of a
  kit take its setup along. A kit or project with no setup starts with a copy of the one on. It is an optional
  feature: a manifest fragment and a header the glue calls through (README.md, "Features").

## 5. Architecture

```text
digi_macro
  src/mm_logic.c   state machine, input map, macro math, storage layout   portable C, no firmware calls (host-tested)
  src/mm_ui.c      ev_key, ev_enc, ev_tick, ev_draw handlers; screens       calls firmware routines by address
  src/mm_apply.c   the per-track lists and the ev_frame_params handler     interrupt level, .run
  core-shaped companions, each in its own file so it can move into core-dt2 / core-dn2 by pull request:
  src/leds_mk2.s   W1  ev_key_leds on the Digitakt II and Digitone II       the same event name and prototype as core-dn1 3.3
  src/params_mk2.s W2  ev_frame_params: the per-frame value arrays          new event
  src/store_mk2.c  W3  global storage (later pattern / project scopes)      core-ar2's storage model
  mods/dt2/mod.json, mods/dn2/mod.json   one manifest per device and OS: sources, routine addresses, sites, claims
```

| Work | What | Known | Spike |
| --- | --- | --- | --- |
| W1 `ev_key_leds` | Hook the per-frame LED pass before the stock views set theirs; handlers get the LedManager; first setter wins | mk1 design and sites (DN mk1 1.43 / 1.44: the pass at `0x400e5d22` / `0x400e5f96`, called at `0x40018332`); digikit's LedManager setter and LED arrays (1.16) | S2 |
| W2 `ev_frame_params` | Hook the frame interrupt where the stock modulators apply into the per-track value array; handlers get the arrays (stride 71 / 101) | digikit finding 04 (kernel, Digitone II driver, LFO tick, 1.16 / 1.11); mk1's `ev_render_params` | S3 |
| W3 storage | Global fields in bytes the stock OS keeps but never reads; later pattern and project | elekloader's storage linker and lockfile (`elekloader/store.py`); core-ar2's sites as the model; digikit's kit-record spare halfwords | S5 |
| W4 destinations | A per-device table of offerable destinations: index, short and long name, group, per machine | digikit finding 02 (descriptor table, (index, machine) → code), finding 71 (`ModDestListView`) | S6 |

Each companion's sites are declared in the manifest with their stock bytes, so elekloader's linker and `lint`
refuse a build on any other OS file.

## 6. Research spikes (P0)

All are static first, using digikit's tools on DT2 1.17 and DN2 1.12: `tools/coldfire/fw.py`, cfdb, `refscan.py`,
and the hash anchors in `devices/ports/`. They are then checked in digikit's emulator. Results go to
`docs/findings/` here, with digikit's marks and a second check before **[V]**.

| Spike | Questions | Done when |
| --- | --- | --- |
| S1 Input | The `ev_enc` id of LEVEL. Push codes 41–48 arrive at `ev_key`. The stock push + turn step and push + NO reset (D3). What the stock does with the first key of the entry chord, which the mod passes on (a PAGE press, a MOD press), and that neither order of PAGE + MOD has a stock use. | A table of events and stock behaviours, both devices |
| S2 LEDs | The per-frame LED pass on both devices (port the mk1 pass by masked code, as elekloader found its other mk2 sites; cross-check with digikit's LedManager setter). Whether the playhead and the view LEDs all go through it. Which value is blue: look for the panel MCU's firmware in the image (digiemu's lead), else a unit | Site, prototype and colour values, both devices |
| S3 Modulation | Resolve the modulation kernel, the drivers and the LFO tick on 1.17 / 1.12; find the Digitakt II's driver and sources. Find the per-frame working array the modulators write and prove it is rebuilt from the base values every frame (the Digitakt II's `0x8e` mirror is updated only from dirty bits, so this is not obvious). Find a site before the modulators on both devices and measure the frame's spare time | A go / no-go note with the sites; **no-go stops the plan** (D1) |
| S4 Screens | The stock modulation setup views' layout and fonts (the Digitone II's six `*SetupView` classes, the Digitakt II's equivalent). Text and invert routines and fonts on DN2 1.12. The cost of a full redraw | Routine addresses for both manifests and a layout to copy |
| S5 Storage | A global record the stock OS saves and loads with bytes it never reads, on both devices; then the pattern and project records (for D4 later). Else a file on the +Drive through the firmware's file API | Location, size and the save and load paths |
| S6 Destinations | The stock destination list per device and per machine (what the modulation setups offer) and its names | A generated table for each device |

## 7. Phases

| Phase | Work | Done when | Days |
| --- | --- | --- | --- |
| P0 | Spikes S1–S6 (parallel with M1) | Findings and the S3 go / no-go | 4–6 |
| M1 | **Walking skeleton** on the Digitakt II: PAGE + MOD (either order) opens the knob screen, A–H turn (push for bigger steps, push + NO resets), MOD exits. Needs only the hooks that exist | Runs in digikit's emulator from an elekloader build | 2–3 |
| W1 | LED event and the mode's LEDs, both devices | MOD blue; trig keys red in SETUP with PLAY running | 2–4 |
| W2 + W4 | Per-frame event, macro math, destination table | A macro moves its target on the masked tracks only; kit bytes unchanged; frames identical with every macro at the centre | 6–10 |
| SETUP | The D7 setup screen, slot selection, masks on the trig keys, NO back | Scenario: configure C slot 2 on tracks 3 and 5, turn C, check both | 3–4 |
| W3 | Global storage | The configuration survives a restart; a project saved with the mod loads on the stock OS | 3–5 |
| DN2 | The Digitone II manifest: every address and site again | The same scenarios pass on DN2 1.12 | 2–3 |
| Test | Scenarios on both devices in digikit's emulator, then units (Digitakt II first) | Section 8 | 3–5 |
| Upstream | Pull requests to elekloader moving W1–W3 into `core-dt2` / `core-dn2`; the mod into the shop | Opened, with their checks | 1–2 |

Total: 26–42 days. M1 comes first on purpose: it shows the UI half working in the first week, while P0 settles the
risky half.

## 8. Testing

- **Unit (host).** `mm_logic.c` built for the host. Cases:
  - entry in both orders, on the second key's press; a MOD press that exits does not enter again when PAGE
    follows while MOD is held;
  - exit on MOD's press from the macro screen; MOD, with or without PAGE held, back from the setup screen, so two
    MOD presses leave from there; PAGE + MOD exits from the macro screen;
  - every taken press has its repeats and release taken too, and a passed press its release passed;
  - push + turn steps, and push + NO resets;
  - clamping at both ends, and ±64 × ±64 without overflow;
  - two slots on one destination are merged;
  - a storage layout from a future version is refused.
- **elekloader's checks.** `python -m elekloader.sdk.build`, `python -m elekloader.lint <mod> --stock <syx> --with
  <core> --json` (sites, claims, `core_candidates`) and `python -m elekloader.patch`, for each device.
- **Emulator.** digikit's emulator boots the built `.syx`: `mise run workbench -- macro-dt2.syx` by hand, or its
  daemon scripted through the control protocol, as elekloader's own Digitone II tests do (`ELEKLOADER_DIGIKIT`,
  `ELEKLOADER_DIGI_EMUD`). The protocol calls:
  - `input.button(code, down)`, for chords with held keys;
  - encoder turns;
  - `framebuffer.get` with a hash, for screen goldens;
  - `leds.get`, for the MOD and trig LEDs.

  The parameter effect is read from the frame and the value arrays (digikit's frame tools): the target moves on the
  masked tracks only, the kit bytes stay the same, and with every macro at the centre the frames equal those of a
  build without the mod. Turns are checked for direction and range, not exact values; the emulator's encoder
  mapping is **[O]** in digikit's finding 58.
- **Units.** The emulator has no panel MCU, so these are checked on hardware: the real colours, responsiveness with
  PLAY running, no audio glitch while turning, PAGE + MOD with real key bounce, and the recovery path (FUNC at power
  on, TRIG 4, the stock `.syx` over DIN MIDI).

## 9. Risks

| Risk | Effect | Mitigation |
| --- | --- | --- |
| S3 finds no hook before or among the modulators | D1 cannot be met | Stop and re-plan with the user; no silent fallback to writing the kit |
| Interrupt-level cost | Audio glitches | Precomputed lists, no work at the centre, the spare time measured in S3 |
| Offsets change at the UI tick (about 30 Hz) | Zipper noise on cutoff or level | Check whether the DSP smooths these parameters; if not, ramp the deltas in the frame handler |
| The Digitakt II and Digitone II cores are unreleased and experimental | Core changes under us | Pin the elekloader commit; declare `resources.core`; offer W1–W3 upstream early |
| Other mods' key handlers | Lost or doubled keys | Low `order`, claimed names, pass everything the input map does not take |
| LED colour values inferred | Wrong colours on a unit | S2 (embedded panel firmware), then a unit |
| An address wrong on one device | Crash or no effect | Every site carries its stock bytes; hash anchors and a second check in P0 |
| Storage bytes the stock OS does use after all | Corrupted projects | Elekloader's rule (bytes the stock never reads or writes) and a round trip through the stock OS |
| Licence | Distribution | GPL-2.0-or-later, as elekloader. Nothing is copied from digikit (AGPL-3.0-or-later); only its tools and published facts are used |

## 10. Repository layout

```text
SPEC.md, PLAN.md, README.md, LICENSE (GPL-2.0-or-later)
third_party/elekloader/        git submodule, pinned (the build, lint and patch tools; core-dt2, core-dn2)
src/                           mm_logic.c, mm_ui.c, mm_apply.c, leds_mk2.s, params_mk2.s, store_mk2.c, mm.h
mods/dt2/mod.json              device digitakt-mk2, os 1.17: sources, cflags with routine addresses, subscribe, sites, claims
mods/dn2/mod.json              device digitone-mk2, os 1.12
tests/test_logic.py            host build of mm_logic.c
tests/emu/                     scenarios against digikit's emulator (a digikit checkout and its daemon, given by path)
docs/findings/                 S1-S6 results
.gitignore                     firmware/, out/, *.syx, *.elemod: never commit firmware or builds
```

## Appendix: the digikit route (revision 1)

Revision 1 (commit `4dba5d6`) built macro mode as a digikit run-time module on five new digikit capabilities: an
input filter, module-owned views and drawing, LED overrides, a parameter catalogue with a modulation source, and
module settings. It would load from the card without reflashing, and it would use digikit's regression harness. It
needs all five built first, of which module-owned views carry the most risk, and needs them accepted into digikit.

Choose it over this plan only if card-loaded updates or staying inside the digikit runtime matter more than reusing
hooks that already run on units. Spikes S2–S6 and the W2 design carry over unchanged.

[el]: https://github.com/elekloader/elekloader/tree/a401967
[el-adapt]: https://github.com/elekloader/elekloader/blob/a401967/docs/ADAPTING.md
[el-dt2]: https://github.com/elekloader/elekloader/blob/a401967/mods/core-dt2/README.md
[el-dn2]: https://github.com/elekloader/elekloader/blob/a401967/mods/core-dn2/README.md
[el-pd]: https://github.com/elekloader/elekloader/blob/a401967/examples/perform-direct/README.md
[el-hc]: https://github.com/elekloader/elekloader/blob/a401967/examples/hooks-check-dn2/README.md
[de]: https://github.com/irpina/digiemu/tree/f57c2f2
[de-led]: https://github.com/irpina/digiemu/blob/f57c2f2/emu/panelleds.py
[dk]: https://github.com/toonst/digikit/tree/5e62a52e8a7d
[f02]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/02-machines-and-parameters.md
[f03]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/03-ui-and-panel.md
[f04]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/04-coldfire-dsp-link.md
[f19]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/19-sequencer-and-leds.md
[f61]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/61-fx-pages.md
[f71]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/71-ui-components-1-17.md
