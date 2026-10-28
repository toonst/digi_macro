# Macro mode: implementation plan

Status: plan, 2026-10-28. Requirements: [SPEC.md](SPEC.md). Reference: [toonst/digikit][dk] at `5e62a52` (main).
Targets: **Digitakt II OS 1.17** and **Digitone II OS 1.12**, the versions digikit analyses and patches.

Evidence marks follow digikit: **[V]** verified by a second agent against image bytes, **[D]** read or measured once,
**[O]** open. Addresses quoted from digikit findings for DT2 1.16 / DN2 1.11 are references only: they must be
resolved on 1.17 / 1.12 (by hash, through `devices/ports/`) before use.

## 1. Summary

Macro mode is built as a **digikit run-time service module**, `toonst/macro`. It is ColdFire C, loaded from the card
as a `.dkm`, and it lives in this repository. It uses only the digikit SDK. Today the SDK has menus, dialogs and
toasts, but macro mode needs five things it does not have. These go into digikit first, as generic adapters and SDK
headers on `toonst/digikit` (written so they can go upstream):

| # | Capability | Why macro mode needs it |
| --- | --- | --- |
| C1 | Input filter | See and swallow MOD + PAGE, MOD, A–H turns and pushes, trig keys and NO before the stock views do |
| C2 | Module-owned views and drawing | A full-screen view with 8 round knobs, and the setup screen with an inverted row |
| C3 | LED overrides | MOD blue while the mode is on; trig keys red for the tracks of the selected slot |
| C4 | Parameter catalogue and a module modulation source | Pick destinations; apply 4 parameters × N tracks per macro |
| C5 | Module settings | Keep the macro configuration across restarts |

Macros are applied **without changing the kit**. Each one works as an extra modulation source next to the
firmware's own (velocity, aftertouch, LFO). The firmware computes those on the ColdFire and adds them to the
per-track value array before it builds the DSP frame ([finding 04][f04], "modulation kernel"). So a macro at its
centre leaves the sound and the kit exactly as they were, nothing is saved by accident, and **no SHARC code
changes**, which also avoids digikit's costly SHARC regeneration path. If the research spike (P0, S3) shows the
modulation path cannot take a hook, the fallback is to write the parameters through the firmware's own setters,
the path an encoder edit takes (section 6, C4 option B).

**Payoff and confidence.** The payoff is a performance feature the stock OS does not have, on both devices, with no
DSP risk. The generic pieces (C1–C3, C5) can also be reused by any digikit module. Estimated effort: 26–43 working
days (28–43 with option A, 26–39 with option B; section 7). My confidence that the whole feature can be built is
high (about 85 %), because option B always works. My confidence in the non-destructive option A is medium (about
55 %) until spike S3 has run. **Stop and re-plan** if S3 finds no safe hook and the user does not accept the
kit-writing semantics of option B (D1).

## 2. The spec on the device

The spec's words, and what they mean on the panel (digikit `devices/*-leds.toml`, [finding 58][f58],
[finding 19][f19]):

| Spec | Device | Code | LED |
| --- | --- | --- | --- |
| "MOD knob" | MOD **page key** (the LFO page). It is a key with an LED, not a knob | 6 | logical 25, idx 24 |
| "PAGE" | PAGE key | 22 | idx 18 (lit by views) |
| "shift" | **FUNC**. Neither device has a SHIFT key | 17 | idx 2 |
| "pressing the (A–H) knob" | Encoder push A–H | 41–48 | none |
| "turning knobs A–H" | Encoders 1–8 (encoder 9 is LEVEL) | enc 1–8 | – |
| "main keys", tracks 1–16 | Trig keys | 25–40 | idx 25–40 |
| NO | NO | 12 | idx 13 |
| blue / red | Value byte `0x41` / `0x11`. The colour bits are inferred **[O]**: on the device only red RECORD and green PLAY are confirmed | – | – |

Both devices have 16 tracks and the same key codes 1–49. Code 50 is SAMPLING on the DT2 and VOICE on the DN2; the DN2
adds 51–54 (ARP, PLUS, STACK, MINUS). FUNC + MOD is already taken: it opens the Master compressor page
([finding 03][f03]). MOD + PAGE has no documented stock use; spike S1 checks this.

## 3. Decisions to confirm

Each decision has a proposed default. The plan assumes the default until the user says otherwise.

| # | Question | Proposed default |
| --- | --- | --- |
| D1 | Does a macro change the kit (destructive) or apply an offset on top of it (non-destructive)? | **Non-destructive**: centre = stock sound; kits and projects are never changed by a macro |
| D2 | When you leave macro mode, do the macros keep their effect? | **Yes**: positions stay until changed or reset; they are not saved, so every boot starts at the centre ("default state in the middle") |
| D3 | How do you reset a macro? | Push A–H alone (without FUNC) returns that macro to the centre |
| D4 | Where is the configuration kept? | v1: globally in the digikit settings store (survives a restart, the same for every project). Per project is later work (section 9) |
| D5 | Depth range | −64 … +63 per slot, 0 = slot off; at full depth and full turn the offset spans the whole parameter range |
| D6 | Which keys keep their stock function in macro mode? | Transport (PLAY, STOP, RECORD), PTN, SONG, TRK, LEVEL encoder, and the trig keys on the macro screen. The setup screen takes the trig keys for track selection |
| D7 | How are the 4 slots chosen and edited on the setup screen? | UP / DOWN choose the slot (the selected row is inverted); encoder A = destination, B = depth. Match the stock modulation setup's mapping once spike S4 has read it |
| D8 | MOD on the setup screen | Leaves macro mode entirely (MOD always exits). NO goes back to the macro screen, as in the spec |
| D9 | Where can macro mode be entered from? | The main screen and the parameter pages (no menu or dialog open) |
| D10 | MIDI tracks | Left out in v1: they are not offered as targets |
| D11 | Destinations | Per-track sound parameters (SRC, FLTR, AMP, track FX sends and LFO settings), the set the stock modulation destination list offers. Kit-wide FX parameters (delay, reverb, chorus, compressor) are later work |

## 4. Where the code lives

digikit's layers ([ARCHITECTURE.md][arch]) allow a content module (`.dkm`) to call the SDK only. Firmware hooks
belong in adapters, and anchors in `devices/ports/`. Macro mode follows the same rules:

```text
toonst/digi_macro  modules/macro          toonst/macro: state machine, screens, macro math, persistence (C, SDK only)
        |  <dk/ui.h> <dk/view.h> <dk/input.h> <dk/led.h> <dk/param.h> <dk/mod.h> <dk/settings.h>
        v
toonst/digikit     sdk/                   the new headers and sdk.toml [[import]] entries
                   modules/core/runtime   input filter at the UI-loop pop; module settings imports
                   modules/core/adapters  ui (views, drawing), led (new), modsrc (new: catalogue, modulation source)
                   devices/fw, ports      declarations and anchors for DT2 1.17 and DN2 1.12
```

- **digi_macro** holds the module, its tests, its docs and a pinned digikit (git submodule `third_party/digikit` on a
  `toonst/digikit` branch). `PATCHKIT_MODULES=$PWD/modules` lets digikit's `module:check`, `module:pack`, `dk:dev`
  and `dk:run` find the module ([MODULE-AUTHORING.md][auth], section 3).
- **digikit** holds C1–C5. They are generic (no macro-specific code), so they can be offered upstream to
  `ElekLabs/digikit`. C2 is already step 9, item 5, of digikit's [SDK plan][sdkplan] ("module-owned views").

Alternatives considered:

1. **Everything in digi_macro as a patched-in module with its own `[[op]]` hooks.** Rejected: it breaks the
   layer rules, it duplicates anchors, and every firmware update would land in this repository.
2. **A core app in digikit** (`modules/core/apps/macro`). This would work, and it is a reasonable choice if the user
   prefers the mode to ship inside digikit. But then this repository would hold only the spec.
3. **A macro machine on a dedicated track.** The spec already rejects it for now.

The flashed image is digikit's device patch set plus the new adapters. The macro module is ticked in
SETTINGS > DIGIKIT > MODULES, like `digikit/remote`.

## 5. Behaviour

### 5.1 States

```text
          MOD held + PAGE                       FUNC + push A..H
  OFF  ------------------->  MACRO  ------------------------------->  SETUP(k)
   ^                          |  ^                                       |
   |        MOD (alone)       |  |               NO                      |
   +--------------------------+  +---------------------------------------+
   ^                                                                     |
   +----------------------------- MOD (D8) ------------------------------+
```

- **Entry.** MOD is pressed first, and the firmware switches to the LFO page on the press (it cannot yet know a
  chord is coming). The filter notes that MOD is held. When PAGE is pressed while MOD is held, the filter swallows
  PAGE, opens the macro view and also swallows the MOD release. The other order (PAGE held, then MOD) works too,
  and then the MOD press is swallowed before the firmware switches page. The page the user lands on when leaving is
  therefore the LFO page; restoring the previous page is optional polish (it needs the page variable, [finding 71][f71]
  section 7).
- **Exit.** In MACRO or SETUP, MOD is swallowed. On its release, macro mode ends if no other key was pressed while
  MOD was held, so MOD + key chords stay free for later.

### 5.2 Input map

| Input | MACRO | SETUP(k) |
| --- | --- | --- |
| Encoder A–H turn | Move macro A–H, −64 … +63, with simple acceleration | A: destination of the selected slot; B: depth (D7) |
| Push A–H | Reset that macro to the centre (D3) | – |
| FUNC + push A–H | Open SETUP for that macro | Switch to that macro's setup |
| UP / DOWN | Pass through | Select slot 1–4 (the inverted row) |
| Trig 1–16 | Pass through (play, mute, pattern change keep working) | Toggle that track in the selected slot's mask |
| YES | Pass through | Open the destination list for the selected slot |
| NO | Pass through | Back to MACRO (the configuration is saved) |
| MOD | Exit (5.1) | Exit (D8) |
| PLAY, STOP, RECORD, PTN, SONG, TRK, LEVEL | Pass through | Pass through |
| Anything else | Swallowed | Swallowed |

### 5.3 Data

```c
typedef struct { uint8_t dest; int8_t depth; uint16_t tracks; } mac_slot; /* dest = value-array index, 0 = none */
typedef struct { int8_t pos; mac_slot slot[4]; } macro;                  /* pos -64..+63, 0 = centre */
static macro macros[8];                                                   /* about 140 bytes, module static data */
```

Each slot has its **own** track mask: "for the selected parameter, to which tracks it applies".

### 5.4 Screens (128 × 64, 1 bpp)

```text
 MACRO screen                              SETUP screen (macro C)
 +------------------------------------+    +------------------------------------+
 |   (|)     (\)     (|)     (/)      |    | MACRO C                            |
 |    A       B       C       D       |    |  1 FLT FREQ   +40    T1 T2 T5      |
 |   (|)     (|)     (|)     (|)      |    | [2 AMP DEC    -12    T3 .. T16    ]|  <- selected: inverted
 |    E       F       G       H       |    |  3 SMP TUNE    +5    8 TRACKS      |
 |                         C:+23      |    |  4 ---          0                  |
 +------------------------------------+    +------------------------------------+
```

- Knob: a circle of radius about 9 px with a pointer. Pointer up = centre; the travel is ±135°. Under each knob is
  a short label: the letter, or the first slot's short name once it is set. While a knob turns, the bottom line
  shows `C:+23`.
- If spike S4 finds the stock knob widget (the parameter pages draw round knobs), C2 exposes it, and the macro
  screen looks like a stock page. Otherwise the module draws knobs from pixels (integer sine table, no FPU).
- The SETUP layout copies the stock modulation setup (DN2: the six `*SetupView` classes, [finding 04][f04]; the DT2
  equivalent, S4).

### 5.5 LEDs

- MACRO and SETUP: the MOD LED is held at blue (`0x41`).
- SETUP: the trig LEDs show the selected slot's mask: red (`0x11`) for an included track, off (`0x01`) for the
  others. The sequencer's playhead must not overwrite them while SETUP is open (C3).
- On exit, every overridden LED goes back to the value the firmware last asked for.

### 5.6 Applying a macro

For a macro at position `p` and a slot with depth `d`, on every track in the slot's mask:

```text
delta   = (p * d * 0x7f00) >> 12           /* |p*d| <= 4096: at full turn and full depth, delta spans the range */
applied = clamp(base + delta, 0, 0x7f00)   /* raw parameter units; the UI shows raw / 256; the clamp is the stock modulators' */
```

- `base` is the value after the stock modulators (option A) or the kit value (option B). Bipolar parameters are raw
  0 … 0x7f00 too, with the centre at 0x4000, so the same formula holds.
- The module computes the per-track `(dest, delta)` lists on the UI task (about 120 Hz), merging slots that hit
  the same destination. C4 only adds them. So the per-frame work is at most one add and one clamp per active
  (track, destination) pair (at most 16 × 32), and none when all macros are at the centre.
- Destinations are value-array indices (DT2: 71 per track, DN2: 101; the modulation bounds are 70 / 100). The
  same index means different machine parameters on tracks with different machines. The setup screen names a
  destination by the machine of the first track in the mask (or the active track), like the stock list.

### 5.7 Persistence

- One settings key, `toonst/macro/config` (keys `<module id>/...` are reserved for modules in digikit's
  `sdk/sdk.toml` `[settings]`). The value is a versioned blob: magic, version, then 8 × 4 × {dest u8, depth s8,
  tracks u16}, about 134 bytes. The limit is 1024 bytes.
- The store writes 2 s after the last change, to the copy that is not current ([finding 63][f63]), so saving on
  every NO is cheap for the card.
- Macro positions are not saved (D2).
- `dkm_menu` (SETTINGS... in DIGIKIT > MODULES) has RESET MACROS, CLEAR CONFIGURATION and a help text page, built
  with the existing `<dk/ui.h>`.

## 6. digikit capabilities to add

Each capability follows digikit's rules. Anchors are declared in `devices/fw/*.toml` and resolved into the port
files; `patchkit gen` generates the bindings; the adapter is one C source per capability where it can be; each
new import is listed in `sdk/sdk.toml`, and when the adapter is absent its call returns a failure value
(`absent`); and the proof is an emulator run on both devices, which must be bit-identical when the capability is
unused.

### C1 Input filter: `<dk/input.h>`

```c
typedef struct { uint8_t kind; uint8_t code; uint16_t flags; int16_t delta; } dk_input_event; /* KEY or TURN */
typedef int dk_input_fn(void *ctx, const dk_input_event *e);   /* return 1 to swallow the event */
int dk_input_filter(dk_input_fn *fn, void *ctx);                 /* one per module, called on the UI task */
int dk_input_is_down(int code);
```

- **Where.** `core/runtime` already retargets the `jsr` that pops the UI task's next event (`cf.view.hook.ui_loop`,
  anchored on both devices). Extending that hook so it pops, offers the record to the filters and drops a swallowed
  one covers keys and encoders, before any view sees them, at one site that is already anchored.
- **Known.** A record is 16 bytes: `+0` type (0 key, 1 encoder, 5 the DTIM3 tick), `+4` code, `+8` flags, `+0xc` a
  timer ([finding 03][f03], [finding 58][f58]; **[V]** for keys and the tick, **[D]** for encoders, [finding 71][f71]). Key flags: `0x01` press, `0x02` chord (a modifier is
  held), `0x08` repeat, `0x10` release; finding 71 section 8 calls some bit names **[O]**. The queue holds item
  pointers, not copies. `dk_input_is_down` reads the previous-mask state that `core/input` already anchors
  (`cf.input.site.key_decoder.prev_mask_lit`).
- **Spike S1.** The fields of an encoder record. Who frees a popped item, so that dropping one does not leak or
  double-free; the alternatives are to pop again or to turn the record into an event the firmware ignores. The
  stock handling of MOD press, repeat and release, and of MOD + PAGE. Whether a swallowed release leaves the
  chord state consistent.
- **Proof.** A fixture module swallows one key and one encoder. The screen hash and the LED state are unchanged by
  those inputs, other keys still work, and with no filter registered the frames are bit-identical to an image
  without it.

### C2 Module-owned views and drawing: `<dk/view.h>`

```c
typedef struct {
    const char *name;
    void (*draw)(void *ctx, dk_ui_canvas *c);
    int  (*key)(void *ctx, int code, int flags);   /* 1 = handled */
    int  (*turn)(void *ctx, int enc, int delta);
    void (*tick)(void *ctx);
    void (*closed)(void *ctx);
    void *ctx;
} dk_ui_view_desc;
dk_ui_view *dk_ui_view_new(const dk_ui_view_desc *d);  int dk_ui_view_open(dk_ui_view *v);
void dk_ui_view_close(dk_ui_view *v);                  void dk_ui_view_invalidate(dk_ui_view *v);
void dk_ui_px(dk_ui_canvas *c, int x, int y, int on);  void dk_ui_line(...);  void dk_ui_fill(...);
void dk_ui_invert(...);  int dk_ui_print(dk_ui_canvas *c, int font, int x, int y, int flags, const char *s);
int dk_ui_text_width(int font, const char *s);       /* optional: void dk_ui_knob(c, x, y, value, bipolar) */
```

- **Known.** [Finding 71][f71] sections 2, 3, 6 and 9 have most of it. Of the View vtable: key slot 2, draw slot 4,
  the "opaque" flag at slots 8 and 9 (views below the topmost opaque view are not drawn), close slot 10 and tick
  slot 11. Push is `cf.view.fn.view_controller_push` and invalidate is anchored. Drawing: text, `fill_rect`,
  invert, blit, text width; the screen Bitmap is 128 × 64 at 1 bpp in bands of 32 rows, data pointer at `+0x14`.
  `core/ui` already builds views of its own: it copies the 17-slot View vtable, replaces slots 2 and 4, and makes
  the `shared_ptr` with `cf.string.fn.shared_ptr.make`. So C2 is an extension of `core/ui`, not a new adapter.
- **Spike S4.** The View encoder slot (untraced) and the LedHandler slot. The bit order of the Bitmap data, so that
  `dk_ui_px` writes words directly instead of calling `fill_rect` for single pixels. The fonts (only the pointer
  `0x402a9be0` is known). The stock knob widget that the parameter pages draw. The layout and controls of the stock
  modulation setup view on both devices. Size-only DN2 pairs (render, key and encoder dispatch) need a hash check.
- **Proof.** An extension of the `test/ui-demo` fixture opens a view that draws a knob and swallows keys. Golden
  screenshots on both devices (`dk:dev ... shot:NAME`), NO closes it, and no fault after 100 open/close cycles.

### C3 LED overrides: `<dk/led.h>`

```c
enum { DK_LED_OFF = 0x01, DK_LED_RED = 0x11, DK_LED_GREEN = 0x21, DK_LED_BLUE = 0x41, DK_LED_WHITE = 0x71 };
int  dk_led_hold(int led, int value);   /* override until released; led ids from the device profile */
void dk_led_release(int led);          void dk_led_release_all(void);
```

- **Known (1.16 reference addresses).** Every LED change goes through one record builder, `FUN_40133d64(logical,
  value)`, which writes `b9 idx val`. The firmware keeps a shadow value, a dirty bitmap and a pulse state per LED,
  and sets LEDs through `FUN_400308e0(LedManager, logical, layer, value)`, where a nonzero layer blocks the next
  set ([finding 19][f19]). The idx per key is in `devices/*-leds.toml`.
- **Plan.** Hook the record builder. While an LED is held, record the value the firmware asked for and send the
  override instead. On release, send the recorded value. This works whoever drives the LED (views, sequencer
  playhead, pulses).
- **Spike S2.** Resolve the builder, setter and state arrays on 1.17 and 1.12. Find out how the shadow and dirty
  logic interacts with a substituted value, so the firmware does not skip a later real change. Check that the
  stock Control All configuration and compressor routing screens light the trig keys red through the same path
  (the spec's model for this behaviour).
- **Proof.** `leds.get` from the emulator control protocol (and `DIGI_PANEL_TX_LOG` with
  `tools/emulator/led_map.py`): the held value stays while PLAY runs the playhead over the trig keys, and the stock
  values return after release.

### C4 Parameter catalogue and modulation source: `<dk/param.h>`, `<dk/mod.h>`

```c
int dk_param_dests(int track);                                         /* how many destinations */
int dk_param_dest(int track, int i, dk_param_info *out);               /* index, name, short, group, bipolar */
int dk_mod_set(int track, const dk_mod_pair *pairs, int n);            /* (dest, delta) for one track */
int dk_mod_commit(void);                                               /* publish all tracks at once */
```

- **Catalogue, known.** The parameter descriptor table: codes, mirror indices, ranges, names, owners
  (`paramtable.find_table` finds it on all four images; [finding 02][f02]). The machine-dependent mapping from
  (index, type) to code (DT2 1.17 `0x400da3a2`). The stock destination list `ModDestListView` ([finding 71][f71] section 4).
- **Option A (non-destructive), known (1.16 / 1.11).** The modulation kernel applies one source through four
  destinations `depth << 16 | dest` and saturates the result (DT2 `0x400d9354`, DN2 `0x400db1dc`); the LFO apply clamps
  to 0 … 32512. The DN2 driver
  `0x400db22c` runs six sources × 4 × 16 tracks. The LFO tick runs once per frame (DT2 `0x40139342` from
  `0x4002e91c`) ([finding 04][f04]).
- **Where.** A new adapter `core/modsrc` owns the catalogue and the modulation source.
- **Option A, plan.** `core/modsrc` hooks the frame path right after the stock modulators. It adds
  the module's per-track offsets from a double-buffered table: the module writes the back buffer on the UI task
  and `dk_mod_commit` flips an index word. The frame path never calls module code.
- **Spike S3 (the go / no-go for option A).** Resolve the kernel, the drivers and the LFO tick on 1.17 and 1.12.
  Find the DT2 driver and its sources. Find which array the modulators write and prove it is rebuilt from the base
  values every frame (the `0x8e` mirror at `0x80003362` is updated only from dirty bits, so this is not obvious).
  Find a hook site after the last modulator on both devices. Measure the per-frame cost.
- **Option B (fallback).** `dk_param_get/set(track, dest, raw)` through the firmware's own setters
  (1.16: `SoundParameterSet::vfunc_13/31` -> `FUN_4002d7a4`, the path an encoder edit takes) on the UI task. The module
  records the base value when a macro leaves the centre and writes it back at the centre. Limits: the kit changes
  (and can be saved), and an edit of the same parameter while the macro is off centre is overwritten.
- **Proof.** With all offsets zero, or the module not loaded, the frames and PCM are bit-identical. With an offset
  on track 1 cutoff, only that word changes, by the expected amount. Option A: the kit's Sound array and a kit
  save are unchanged.

### C5 Module settings: `<dk/settings.h>`

```c
int dk_settings_get(const char *key, void *dst, int cap);   /* key is relative: "config" -> "toonst/macro/config" */
int dk_settings_set(const char *key, const void *src, int n);
```

- **Known.** The store, the C ABI `dkrt_settings_*`, the two copies, the delayed write and the SysEx `SETTINGS`
  command exist ([finding 63][f63]). `sdk/sdk.toml` reserves `<module id>/...` keys but says "no module API yet".
- **Plan.** Imports that prefix the calling module's id, so a module cannot reach other owners' keys.
- **Proof.** Set, restart, get on both devices; a key of another owner is refused.

## 7. Phases

| Phase | Repository | Work | Done when | Days |
| --- | --- | --- | --- | --- |
| P0 | digikit | Static spikes S1–S5 on DT2 1.17 and DN2 1.12 with `tools/coldfire/fw.py`, cfdb and `refscan.py`. S5: the destination list and names per machine. Results go into `docs/findings/` (one topic per file, indexed) | A findings file per spike and a written go / no-go for option A (D1) | 3–5 |
| P1 | digikit | C1 input filter, C3 LED overrides; SDK headers, `sdk.toml` imports, fixtures | C1 and C3 proofs on both devices | 4–6 |
| P2 | digikit | C2 views and drawing in `core/ui` | C2 proof; golden screenshots | 4–6 |
| M1 | digi_macro | **Walking skeleton.** `module:new service toonst/macro --dir modules`; MOD + PAGE opens the knob screen, A–H turn the knobs, MOD exits, MOD LED blue | `dk:dev` sheet on DN2, then DT2 | 2–3 |
| P3 | digikit | C4 (option A, or B after P0) and C5 | C4 and C5 proofs | 5–8 (A) / 3–4 (B) |
| P4 | digi_macro | Macro math (5.6), per-track offset lists, persistence, `dkm_menu` | Host unit tests; a turned macro changes the target parameter on the selected tracks only | 3–4 |
| P5 | digi_macro | SETUP screen: slot selection, destination list, depth, trig-key masks with red LEDs, NO back | Scenario: configure C slot 2 on tracks 3 and 5, turn C, check both | 3–4 |
| P6 | both | Acceptance scenarios on both devices (section 8); then the device rung on a Digitakt II with the runtime flashed (`dk:run --port Digitakt`) | Scenarios green on DT2 1.17 and DN2 1.12; device check list signed off | 3–5 |
| P7 | both | Player README, `module:export` `.dkmod`, gallery card; digikit PRs for C1–C5 with the gate (`mise run verify -- --base main`) | Released | 1–2 |

Total: 26–43 days (28–43 with option A, 26–39 with option B). M1 comes before C4 on purpose, to show early that the UI half works.

## 8. Testing

Test levels follow [digikit's testing guide][testing].

- **Unit (host, no device).** The module's logic is split from its SDK calls: the state machine and input map, the
  macro math and merging, the settings blob codec, the knob raster. These pure files are compiled for the host and
  driven from `modules/macro/tests/test_*.py` (the folder `module:check` runs), like digikit's C unit
  `tests/regression/modules/remote-pacing.c`. Cases: the entry chord with and without other keys, exit on MOD
  release only, clamping at both ends, ±64 × ±64 without overflow, two slots on one destination merged, a blob
  from a future version refused.
- **Iteration.** `PATCHKIT_MODULES=$PWD/modules mise run dk:dev -- toonst/macro --device dn2 --keys '...'`. Chords
  need a held key: `dk:dev` inline scripts know `tap`, `wait`, `turn` and `shot`. Either use JSON steps with
  `input.button(code, down)` or add `down:` / `up:` steps to `pk_devreload.py` (a small digikit change; modifiers
  must be latched, [finding 03][f03]).
- **Acceptance (emulator, both devices).** A Vitest scenario in digikit's style (`withPatchedSession`,
  `installModule`, `tap`, `frameHash`, `ledsGet`): MOD + PAGE gives the golden macro screen and MOD LED `0x41`;
  turning C changes the target value on the selected tracks only; FUNC + push C gives the setup screen; trig 3
  gives red on idx 27 and track 3 in the mask; NO goes back; MOD exits with the stock LEDs and screen back; after a
  restart the configuration is still there and the knobs are at the centre. Option A also checks the kit bytes are
  unchanged. Check the direction and range of a turn, not exact values: the encoder accelerates and the emulator's
  detent-to-value mapping is **[O]** ([finding 58][f58]).
- **Device (DT2 first).** The emulator has no panel MCU, so these are checked on hardware: blue and red on the
  real LEDs; responsiveness while PLAY runs; no audio glitch while turning a macro; MOD + PAGE with real key
  bounce.
- **Gate.** The digikit capability PRs go through `mise run verify -- --base main` once each. In digi_macro, CI
  runs the host unit tests; emulator runs need firmware the user supplies, so they stay local.

## 9. Risks

| Risk | Effect | Mitigation |
| --- | --- | --- |
| S3 finds no safe hook after the modulators | Option A impossible | Option B (C4); decide D1 with the user at the end of P0 |
| Code on the frame path is too slow | Audio glitches | Precomputed offsets, no module code in the frame path, cost measured in S3 (budget: under 1 % of a frame) |
| Zipper noise: offsets change at the UI rate (about 120 Hz) | Audible steps on cutoff or level | Check whether the DSP smooths these parameters; if not, ramp the offset across frames in `core/modsrc` |
| The sequencer repaints trig LEDs | Track mask hard to read | C3 overrides at the record builder, not at the setter |
| Dropping a popped event leaks or double-frees it | Crash after many key presses | S1 reads who frees items; a soak test of 10 000 swallowed events |
| Stock semantics of MOD (page switch on press, repeat) | Entry or exit feels wrong | Swallow the MOD release only after a chord; S1 reads the stock handler |
| DN2 anchors that matched by size only | Wrong code hooked on DN2 | Hash-check every anchor in P0; AGENTS rule: a second agent before **[V]** |
| A destination means a different parameter on tracks with different machines | Confusing names in SETUP | Name by the first selected track's machine; mark slots whose tracks mix machine types |
| digikit upstream does not take C1–C5 | A fork to maintain | Keep them generic and small; pin the submodule to a `toonst/digikit` branch |
| Licence | digikit is AGPL-3.0-or-later with NOTICE terms | License this repository AGPL-3.0-or-later and keep the attribution in the README and the module's help page |

Later work (not in v1): per-project macro configuration (a kit or project record, as digikit's FX pages use the
spare halfwords of the kit's Fx storage record, [finding 61][f61]); kit-wide FX destinations; MIDI tracks (CC
out); macro names; recording macro moves (the "macro machine" idea in the spec).

## 10. Repository layout (digi_macro)

```text
SPEC.md, PLAN.md, README.md, LICENSE
third_party/digikit/            git submodule: toonst/digikit, branch with C1-C5
modules/macro/                  PATCHKIT_MODULES points at modules/
  module.toml                   id "toonst/macro", type "service", category "ui", requires core/ui, core/led,
                                core/modsrc; ports ["dt2-1.17", "dn2-1.12"]
  src/c/macro.c                 dkm_init, dkm_slot_0, dkm_menu; SDK glue only
  src/c/logic.c, logic.h        state machine, input map, macro math, blob codec (host-testable, no SDK calls)
  src/c/draw.c                  knob screen and setup screen
  tests/test_logic.py           host build of logic.c and its cases
  tests/macro.test.ts           acceptance scenario (runs with digikit's regression helpers)
  README.md                     for players: the keys, the screens
mise.toml                       tasks that set PATCHKIT_MODULES and call into third_party/digikit
.gitignore                      firmware/, out/, *.syx: never commit firmware or derived material
```

[dk]: https://github.com/toonst/digikit/tree/5e62a52e8a7d
[arch]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/ARCHITECTURE.md
[auth]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/MODULE-AUTHORING.md
[testing]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/TESTING.md
[sdkplan]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/plans/firmware-decls-and-sdk.md
[f02]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/02-machines-and-parameters.md
[f03]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/03-ui-and-panel.md
[f04]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/04-coldfire-dsp-link.md
[f19]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/19-sequencer-and-leds.md
[f58]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/58-panel-input-path.md
[f61]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/61-fx-pages.md
[f63]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/63-settings-store.md
[f71]: https://github.com/toonst/digikit/blob/5e62a52e8a7d/docs/findings/71-ui-components-1-17.md
