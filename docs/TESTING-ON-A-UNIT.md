# Testing on a unit

A checklist for trying macro-mode on a real Digitakt II or Digitone II. Each test says what to do and what you should
see or hear. Tick what passes; for anything that doesn't, note what happened (see "Reporting a problem" at the end).

Before you start:

- **Back up your projects** (Elektron Transfer, or save them to another slot). Use a new, empty project for the tests.
- Have the official OS `.syx` at hand to go back (README.md, "Installing").
- Build with the kits feature (the default). Tests marked *(kits)* need it.
- Use a pattern with a few tracks playing a steady sound, so you can hear changes clearly: for example a sustained
  note or a fast hi-hat on tracks 1–3.

## 1. It starts

- [ ] **Boot.** The unit starts normally, the stock screens look as usual, and the stock pages and menus work.
- [ ] **Play for a minute** without touching macro mode: no clicks, hangs or slow screens.

## 2. Entering and leaving

- [ ] **Hold PAGE, press MOD.** The macro screen opens: the header says "Macro Mode", MOD lights blue, the other page
  keys go dark, eight round knobs are shown in the centre.
- [ ] **Leave with MOD.** You're back on the page you came from, and the key LEDs are as before.
- [ ] **Hold MOD, press PAGE.** Opens too. Releasing both keys leaves it open.
- [ ] **Leave with a page key** (TRIG, SRC, FLTR, AMP, FX): that page opens directly.
- [ ] **FUNC + MOD** in macro mode: leaves and opens the compressor/mixer page, as it does anywhere.
- [ ] **TRK + a trig key** in macro mode: leaves, and that track is selected.
- [ ] **With a menu open** (for example SETTINGS), PAGE + MOD does nothing; close the menu and it works.
- [ ] **During playback**: entering and leaving doesn't stop or disturb the sound.

## 3. Turning macros

First give macro A something to do (section 4 has the details): FUNC + push encoder A, encoder A → a filter cutoff
(on the Digitakt II, FLTR's FREQ), encoder E (its depth) to about +60, then trig key 1 only (track 1), then MOD.

- [ ] **Turn A** both ways while track 1 plays. You hear the filter open and close. The knob on screen follows,
  from −64 to +63.
- [ ] **The knob feels like a stock knob:** slow turns are fine steps, fast turns go further.
- [ ] **Push and turn A:** bigger steps.
- [ ] **Push A + NO:** knob A goes back to the centre, and the sound to where it was.
- [ ] **Turn LEVEL/DATA:** all macros move together.
- [ ] **FUNC + NO** (or push LEVEL/DATA): all knobs back to the centre.
- [ ] **The sound isn't changed for good.** Turn A far, leave macro mode, open the FLTR page: FREQ shows its old
  value. Turn A back to the centre: the sound is exactly as before.
- [ ] **Other tracks don't change** (track 2 and 3 sound as before while you turn A).
- [ ] **Stock modulation still works on top:** with an LFO on the filter, turning A shifts the sweep; the LFO keeps
  moving.
- [ ] **At the ends:** turn A all the way with a big depth. The parameter stops at its limit, no jumps or wrap-around.

## 4. Setting up a macro

- [ ] **FUNC + push an encoder** (say C): the setup screen for macro C opens.
- [ ] **Encoders A–D** choose the parameter for slots 1–4. The list shows the parameters by page, like the stock
  modulation setup; turn past the ends and it stops.
- [ ] **Encoders E–H** set each slot's depth, positive or negative; pushed, whole steps.
- [ ] **LEFT / RIGHT** select a slot (the selected one is highlighted).
- [ ] **Trig keys** pick the tracks for the selected slot; they light red. A new slot starts with all tracks.
- [ ] **Push an encoder + NO** clears that slot (no parameter, depth 0).
- [ ] **Two slots on one macro** (say filter + volume, opposite depths): turning the macro moves both at once.
- [ ] **Same parameter on two macros:** both add up.
- [ ] **NO** (or MOD) goes back to the macro screen.
- [ ] **Naming:** FUNC + push and hold an encoder opens the naming screen; the name (up to 5 characters) becomes
  the knob's label.

## 5. Kept after a restart

- [ ] Set up two macros, turn the unit off and on. The setups and names are still there.

## 6. One setup per project or per kit *(kits)*

- [ ] **SETTINGS > PERSONALIZE** has a **MACRO SETUP** row at the bottom, showing PROJECT. YES, LEFT and RIGHT switch it
  between PROJECT and KIT.
- [ ] **Per kit:** set it to KIT. On pattern A01 set macro A to the filter. Switch to A02: it starts with the same
  setup. Change A02's macro A to something else. Back on A01: A01's setup. On A02: A02's.
- [ ] **Per project:** set it to PROJECT. The setup is the project's, whichever pattern plays. Change it. Back to
  KIT: A01 and A02 still have their own setups.
- [ ] **Pattern copy and paste:** per kit, copy A01 (FUNC + RECORD) and paste it on A04 (FUNC + STOP). A04 now has
  A01's macro setup.
- [ ] **Undo** the paste (the stock undo): A04's previous setup comes back.
- [ ] **PASTE KIT** (from the kit menu): copy A01's kit, paste it into A05's kit. A05 gets A01's setup.
- [ ] **Saving:** SAVE PROJECT, change some setups, then reload the project. The saved setups come back, per kit
  and for the project.
- [ ] **Kit menu: SAVE KIT, then LOAD KIT** into another pattern. Note whether the setup comes along (not tested
  before; either way is worth knowing).
- [ ] **No "MACRO SETUP CONFLICT"** message appears in any of the above.

## 7. Projects stay compatible *(kits)*

- [ ] **To the official OS:** save the test project, install the official OS, load it. It loads and plays normally.
  Save it again (as another slot), then install macro-mode again.
- [ ] **Back in macro-mode:** load the project saved by the official OS. It loads; the setups may be gone or kept,
  but there's no conflict message. Your original macro-mode save still has its setups.
- [ ] **Old projects:** load a project made before macro-mode. It plays as before; its kits have no setups yet.

## 8. Long run

- [ ] Play a busy pattern for 10 minutes or more while turning macros now and then, switching patterns and opening
  menus. No crash, freeze, glitch or slowdown.

## Reporting a problem

For each failure, note:

- the device and which test (for example "Digitakt II, 6, PASTE KIT");
- what you did, step by step, and what you expected;
- what happened instead (a photo of the screen helps);
- whether it happens again when you repeat it.

If the unit hangs, power it off and on. If it doesn't start, go back to the official OS (README.md, "Installing").
