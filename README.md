# Elektron Digitakt 2 & Digitone 2 macro-mode

Eight performance macros for the **Elektron Digitakt II** and **Digitone II**. Hold PAGE and press MOD, and encoders
A–H become eight macro knobs. Each macro moves up to four sound parameters, on the tracks you choose, all at once.
They work like the stock modulation: they ride on top of the sound and don't change your kit.

<img src="docs/img/macro-turned.png" alt="The macro screen" width="380">&nbsp;&nbsp;&nbsp;&nbsp;<img src="docs/img/setup.png" alt="Setting up macro C" width="380">

> [!WARNING]
> **Experimental.** Earlier builds have run on a Digitakt II; this one, with the per-kit setups, has so far only run
> in an emulator, and the Digitone II version has never been on a unit. Don't install it on a device you rely on. This is unofficial custom firmware: you use it at your own risk, and
> it is not made or supported by Elektron.

## What you need

- A **Digitakt II on OS 1.17** or a **Digitone II on OS 1.12**. Other OS versions won't work.
- The matching official OS file (`.syx`) from [Elektron's website](https://www.elektron.se/support-downloads).
- A computer with Linux or macOS to build the firmware. There is no ready-made download: the result contains
  Elektron's firmware, so it can't be shared. You build it yourself from your own copy of the OS.

## Using it

**Macro screen**

| Do this | And |
| --- | --- |
| Hold PAGE + press MOD (or MOD + PAGE) | Macro mode opens: MOD lights blue |
| Turn encoders A–H | Move that macro (−64 … +63). Push and turn for bigger steps |
| FUNC + push an encoder | Open setup of that macro |
| FUNC + push and hold an encoder | Name the macro (5 characters) |
| Turn LEVEL/DATA | Move all eight macros together |
| FUNC + NO, or push LEVEL/DATA | All macros back to the centre |
| MOD, or a page key (TRIG, SRC, FLTR, AMP, FX) | Leave macro mode |

**Setup screen**: FUNC + push an encoder opens the setup of that macro.

| Do this | And |
| --- | --- |
| Encoders A–D | Choose what parameters 1–4 control, from a list like the stock modulation setup |
| Encoders E–H | How far each one moves (its depth, positive or negative; push for whole steps) |
| LEFT / RIGHT | Select one of the four parameters |
| Trig keys | Choose the tracks the selected parameter acts on (lit red) |
| NO or MOD | Back to the macro screen |

The macro setup is saved on the unit and comes back after a restart.

### One setup per project, or one per kit

In **SETTINGS > PERSONALIZE > MACRO SETUP** you choose where the macro setup lives:

- **PROJECT** (default): one setup for the whole project.
- **KIT**: each pattern's kit has its own setup, which comes on with the pattern. A kit without one starts as a copy
  of the current setup.

Switching between the two loses nothing: the project and every kit keep their own setup. Copying a pattern or a kit
takes its setup along. The setups are saved inside the project.

## Your projects stay safe

- A project saved with macro-mode still opens on the official OS. It just has no macros there.
- A project from the official OS opens with macro-mode; its kits start without macro setups.
- The macro setups are stored in spare room in the project file that the official OS leaves empty. If something else
  has already put data there, macro-mode leaves it alone and shows **MACRO SETUP CONFLICT**.

## Installing

1. Install Python 3, Git and the ColdFire compiler:
   - Ubuntu or Debian: `sudo apt install python3 git gcc gcc-m68k-linux-gnu binutils-m68k-linux-gnu`
   - macOS: the Xcode command line tools (`xcode-select --install`), then with [Homebrew](https://brew.sh):
     `brew install python m68k-elf-gcc`
2. Get this project and the loader it builds with:

   ```bash
   git clone --recurse-submodules https://github.com/toonst/digi_macro.git
   ```

3. In the `digi_macro` folder, build it, pointing to the official OS file(s) you downloaded (one is enough):

   ```bash
   DT2_SYX=path/to/Digitakt_II_OS1.17.syx DN2_SYX=path/to/Digitone_II_OS1.12.syx tools/check.sh
   ```

   It ends with `WROTE out/check/macro-dt2.syx` (Digitakt II) or `out/check/macro-dn2.syx` (Digitone II).
4. Send that file to the unit as you would an official OS update (see "Updating the OS" in Elektron's manual).
5. Check it worked: hold PAGE and press MOD. The header should say "Macro Mode".

**Going back to the official OS:** send the official `.syx` the same way. If the unit doesn't start, hold FUNC while
powering on, press TRIG 4, and send the official `.syx` over DIN MIDI.

## Good to know

- Macros are for playing live: they can't be recorded or parameter-locked, and they never change the saved sound.
- Machine-specific parameters (on the Digitone II, all synth parameters) and the LFOs can't be chosen as macro
  targets yet.
- Building without the per-kit setups: `MM_KITS=0 tools/check.sh`. Then there's one setup, kept on the unit.

## More

- [docs/DEVELOPING.md](docs/DEVELOPING.md): building, checks, storage details and the code's layout
- [docs/STATUS.md](docs/STATUS.md): what's done and what's open
- [docs/TESTING-ON-A-UNIT.md](docs/TESTING-ON-A-UNIT.md): a checklist for trying it on your unit
- [SPEC.md](SPEC.md) and [PLAN.md](PLAN.md): what it should do and how it was planned

Built with [elekloader](https://github.com/elekloader/elekloader) and tested in
[digikit](https://github.com/toonst/digikit)'s emulator. Licence: GPL-2.0-or-later ([LICENSE](LICENSE)).
