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

## 3. The spec on the device

SPEC.md now uses the panel's names. This table maps its first wording to the panel:

| First wording | Device | Code | LED |
| --- | --- | --- | --- |
| "MOD knob" | MOD **page key** (the LFO page). It is a key with an LED, not a knob | 6 | logical 25, idx 24 |
| "PAGE" | PAGE key | 22 | idx 18 |
| "shift" | **FUNC**. Neither device has a SHIFT key | 17 | idx 2 |
| "pressing the (A–H) knob" | Encoder push A–H | 41–48 | none |
| "turning knobs A–H" | Encoders A–H (LEVEL is a ninth encoder) | enc 1–8 | – |
| "main keys", tracks 1–16 | Trig keys | 25–40 | idx 25–40 |
| NO | NO | 12 | idx 13 |
| blue / red | digikit infers `0x41` / `0x11` **[O]**; S2 confirms | – | – |

Both devices have 16 tracks and the same key codes 1–49. FUNC + MOD is taken (the Master compressor page); MOD +
PAGE has no documented stock use, and S1 checks this.

## 4. Decisions

Confirmed by the user:

| # | Decision |
| --- | --- |
| D1 | Macros apply **on top**, like the other modulations (aftertouch, key tracking, mod wheel): the kit is never changed |
| D2 | Macros **keep their state** when the mode is left |
| D3 | **Push + turn** changes a macro in bigger increments, and **push + NO** resets it to the centre, as for the stock parameters |
| D4 | The configuration is **global** in the first version. Later it is per pattern or per project, chosen by a setting |
| D7 | The setup screen works like the stock modulation setups: **A** = slot 1 destination, **E** = slot 1 depth, **B / F** = slot 2, **C / G** = slot 3, **D / H** = slot 4 |

Still open. The plan assumes each default until told otherwise:

| # | Question | Default |
| --- | --- | --- |
| D5 | Depth range | −64 … +63 per slot, 0 = off; at full depth and full turn the offset spans the whole parameter range |
| D6 | Keys that keep their stock function in macro mode | PLAY, STOP, RECORD, PTN, SONG, TRK, the LEVEL encoder, and the trig keys on the macro screen. They act unseen behind the macro screen |
| D8 | MOD on the setup screen | Leaves macro mode (MOD always exits). NO goes back to the macro screen |
| D9 | Where macro mode can be entered | The main screen and the parameter pages (no menu or dialog open) |
| D10 | MIDI tracks | Not targets in the first version |
| D11 | Destinations | The per-track parameters the stock modulation destination list offers (SRC, FLTR, AMP, track FX sends, LFO); kit-wide FX later |
| D12 | Which slot is selected on the setup screen | The slot whose knob (A–D or E–H) was last turned or pushed; LEFT / RIGHT also step it |
| D13 | Mod id and title | `macro-mode`, "MACRO MODE": not "Digi Macro" (taken in the shop) |
| D14 | Macro positions across a restart | Not stored: every boot starts at the centre, as the spec's default |

## 5. Behaviour

### 5.1 States

```text
          MOD held + PAGE (or PAGE held + MOD)          FUNC + push A..H
  OFF  ------------------------------------>  MACRO  ------------------------>  SETUP(k)
   ^                                           |   ^                               |
   |            MOD alone                      |   |             NO                |
   +-------------------------------------------+   +-------------------------------+
   ^                                                                               |
   +------------------------------------- MOD (D8) -------------------------------+
```

- **Entry.** `ev_key` passes the MOD press on, so the firmware shows the LFO page; it cannot yet know a chord is
  coming. It notes that MOD is down. A PAGE press while MOD is down is taken, macro mode starts, and MOD's release is
  taken too. With PAGE held first, the MOD press itself is taken, so the page does not change.
- **Exit.** In macro mode MOD is taken. On its release the mode ends, if no other key was pressed while MOD was
  down.
- **Claims** (elekloader `resources.names`): `ui:MOD+PAGE`, `ui:macro mode` (the keys it takes while active).

### 5.2 Input map

| Input | MACRO | SETUP(k) |
| --- | --- | --- |
| Turn A–H | Move macro A–H: −64 … +63, one step per detent | A–D: destination of slot 1–4; E–H: depth of slot 1–4 (D7). The turned slot becomes the selected one (D12) |
| Push A–H held + turn | Bigger steps, the stock step (S1 measures it) | The same, on that knob |
| Push A–H held + NO | Reset that macro to the centre | Reset that knob: destination to none, depth to 0 |
| FUNC + push A–H | Open SETUP for that macro | Switch to that macro's setup |
| LEFT / RIGHT | Pass | Step the selected slot |
| Trig 1–16 | Pass (play, mute, pattern change) | Toggle that track in the selected slot's mask |
| NO alone | Pass | Back to MACRO (the configuration is saved) |
| MOD | Exit (5.1) | Exit (D8) |
| PLAY, STOP, RECORD, PTN, SONG, TRK, LEVEL | Pass | Pass |
| Any other key or encoder | Taken | Taken |

`ev_key` runs at a low `order` (before other mods) and `ev_enc` likewise. Both stay `.run` code on the Digitakt II
(a key can come before the first tick).

### 5.3 Data

```c
typedef struct { uint8_t dest; int8_t depth; uint16_t tracks; } mm_slot;  /* dest = value-array index, 0 = none */
typedef struct { int8_t pos; mm_slot slot[4]; } mm_macro;                 /* pos -64..+63, 0 = centre */
static mm_macro mm_macros[8];                                            /* about 140 bytes */
```

Each slot has its own track mask: "for the selected parameter, to which tracks it applies".

### 5.4 Screens (128 × 64, drawn in `ev_draw` over a cleared frame)

```text
 MACRO                                    SETUP, macro C (slot 2 selected: inverted)
 +------------------------------------+   +------------------------------------+
 |   (|)     (\)     (|)     (/)      |   | MACRO C                            |
 |    A       B       C       D       |   | FLT FRQ #AMP DEC# SMP TUN    ---   |  <- A B C D: destination
 |   (|)     (|)     (|)     (|)      |   |   (/)   #  (\)  #   (|)      (|)   |  <- E F G H: depth
 |    E       F       G       H       |   |   +40   #  -12  #    +5       0    |
 |                          C:+23     |   |                                    |
 +------------------------------------+   +------------------------------------+
                                           trig keys: red for slot 2's tracks
```

- **Macro screen.** A circle of about 9 px radius with a pointer. Pointer up is the centre; the travel is ±135°.
  Under each knob is its letter, or the short name of its first slot once one is set. While a knob turns, the
  bottom line shows `C:+23`.
- **Setup screen.** One column per slot: the destination (top, knob A–D) and the depth knob (bottom, E–H). The
  selected slot's column is inverted, as the spec asks. S4 reads the stock modulation setup views so that the layout
  and the fonts match them.
- **Drawing.** Firmware routines by address: `fillRect`, invert and printf text (finding 71; one manifest per
  device). The knob raster is the mod's own (integer sine table). `ev_tick` sets the redraw flag only when the state
  changed.

### 5.5 LEDs

- In MACRO and SETUP the MOD LED is blue.
- In SETUP the trig LEDs are red for the selected slot's tracks, and off for the others. The sequencer's playhead
  does not show there.
- In MACRO the trig LEDs stay the firmware's.
- The handler sets these LEDs from the new `ev_key_leds` (W1) only while the mode is active. With the mode off, it
  sets nothing and the firmware's LEDs return by themselves.

### 5.6 Applying a macro (D1)

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

### 5.7 Persistence (D4)

- **Version 1.** The configuration (8 × 4 slots: destination, depth, mask) goes into a global store (W3) in a
  versioned layout, about 120–140 bytes. It is written when the setup screen closes. Positions are not stored (D14).
- **Later.** The same fields with scope `pattern` or `project`, and a setting (a SETTINGS or PERSONALIZE row from
  `core_additem`) that chooses the scope. This follows elekloader's mod storage, so a project saved with the mod
  still loads on the stock OS.

## 6. Architecture

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

## 7. Research spikes (P0)

All are static first, using digikit's tools on DT2 1.17 and DN2 1.12: `tools/coldfire/fw.py`, cfdb, `refscan.py`,
and the hash anchors in `devices/ports/`. They are then checked in digikit's emulator. Results go to
`docs/findings/` here, with digikit's marks and a second check before **[V]**.

| Spike | Questions | Done when |
| --- | --- | --- |
| S1 Input | The `ev_enc` id of LEVEL. Push codes 41–48 arrive at `ev_key`. The stock push + turn step and push + NO reset (D3). What MOD's press, hold and release do; that MOD + PAGE is unused. | A table of events and stock behaviours, both devices |
| S2 LEDs | The per-frame LED pass on both devices (port the mk1 pass by masked code, as elekloader found its other mk2 sites; cross-check with digikit's LedManager setter). Whether the playhead and the view LEDs all go through it. Which value is blue: look for the panel MCU's firmware in the image (digiemu's lead), else a unit | Site, prototype and colour values, both devices |
| S3 Modulation | Resolve the modulation kernel, the drivers and the LFO tick on 1.17 / 1.12; find the Digitakt II's driver and sources. Find the per-frame working array the modulators write and prove it is rebuilt from the base values every frame (the Digitakt II's `0x8e` mirror is updated only from dirty bits, so this is not obvious). Find a site before the modulators on both devices and measure the frame's spare time | A go / no-go note with the sites; **no-go stops the plan** (D1) |
| S4 Screens | The stock modulation setup views' layout and fonts (the Digitone II's six `*SetupView` classes, the Digitakt II's equivalent). Text and invert routines and fonts on DN2 1.12. The cost of a full redraw | Routine addresses for both manifests and a layout to copy |
| S5 Storage | A global record the stock OS saves and loads with bytes it never reads, on both devices; then the pattern and project records (for D4 later). Else a file on the +Drive through the firmware's file API | Location, size and the save and load paths |
| S6 Destinations | The stock destination list per device and per machine (what the modulation setups offer) and its names | A generated table for each device |

## 8. Phases

| Phase | Work | Done when | Days |
| --- | --- | --- | --- |
| P0 | Spikes S1–S6 (parallel with M1) | Findings and the S3 go / no-go | 4–6 |
| M1 | **Walking skeleton** on the Digitakt II: MOD + PAGE opens the knob screen, A–H turn (push for bigger steps, push + NO resets), MOD exits. Needs only the hooks that exist | Runs in digikit's emulator from an elekloader build | 2–3 |
| W1 | LED event and the mode's LEDs, both devices | MOD blue; trig keys red in SETUP with PLAY running | 2–4 |
| W2 + W4 | Per-frame event, macro math, destination table | A macro moves its target on the masked tracks only; kit bytes unchanged; frames identical with every macro at the centre | 6–10 |
| SETUP | The D7 setup screen, slot selection, masks on the trig keys, NO back | Scenario: configure C slot 2 on tracks 3 and 5, turn C, check both | 3–4 |
| W3 | Global storage | The configuration survives a restart; a project saved with the mod loads on the stock OS | 3–5 |
| DN2 | The Digitone II manifest: every address and site again | The same scenarios pass on DN2 1.12 | 2–3 |
| Test | Scenarios on both devices in digikit's emulator, then units (Digitakt II first) | Section 9 | 3–5 |
| Upstream | Pull requests to elekloader moving W1–W3 into `core-dt2` / `core-dn2`; the mod into the shop | Opened, with their checks | 1–2 |

Total: 26–42 days. M1 comes first on purpose: it shows the UI half working in the first week, while P0 settles the
risky half.

## 9. Testing

- **Unit (host).** `mm_logic.c` built for the host. Cases:
  - the entry chord in both orders, and exit only on MOD's release;
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
  PLAY running, no audio glitch while turning, MOD + PAGE with real key bounce, and the recovery path (FUNC at power
  on, TRIG 4, the stock `.syx` over DIN MIDI).

## 10. Risks

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

## 11. Repository layout

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
