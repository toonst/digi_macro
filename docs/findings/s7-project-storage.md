# S7: macro setups in the project's data (Digitakt II 1.17)

For D4 later (per pattern, per project): can the setups live inside the project, so they travel with it? Marks as
digikit's (**[D]** read or measured once here, **[O]** open). Measured with `tests/emu/zero_patched.rs` (its `poke`
command writes guest memory).

## The patterns in RAM **[D]**

The loaded project's 128 patterns are records of `0x1db8c` (121,740) bytes from `0x41776cb8` (digikit finding 09, the
same on 1.17); the firmware copies whole records for pattern copy and undo. Bytes written into the middle of pattern
1's record (where it holds `0xff`) hung the UI; bytes in the zeros before its last 0x14 bytes did not.

## Saving **[D]**

The Digitakt II has no plain save: SETTINGS > PROJECT has LOAD PROJECT, SAVE PROJECT AS and MANAGE PROJECTS. SAVE
PROJECT AS to slot 001 ("SAVE OK") wrote 28,364 sectors (14.5 MB) from sector `0x58000`, project slot 0 of the 16 MiB
slots (digikit finding 14: slot n at `((n + 0xb) & 0xff) << 15`), and two single sectors (`0x56e43`, `0x56ecb`).
Editing a pattern (a trig) wrote nothing in about 5 G instructions: there was no autosave in that time.

The project is serialized, not copied: 128 pattern records of `0x15c00` (89,088) bytes each (each with a 48 KiB run of
`0xff`), then the kits from about 11.4 MB. Markers written into the zeros at the end of pattern 1's RAM record before
the save were not in the saved data. So bytes of the RAM record the firmware does not use are not saved.

## What is left **[O]**

- A field that is saved and that the firmware does not use, with room for a setup (about 200 bytes a pattern, or one
  per project): none found; the saved format is not mapped (a fresh project's records are mostly defaults, which a
  byte comparison cannot place).
- The end of each project's slot: a save uses about 14.5 of its 16 MiB. A setup kept there needs the mod to write it
  when the firmware saves that slot and read it when it loads one (the save and load paths are not mapped yet), and
  would not travel with Transfer backups or pattern copy/paste unless those are followed too.
