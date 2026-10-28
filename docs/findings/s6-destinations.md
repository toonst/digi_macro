# S6: destinations (Digitakt II 1.17)

Spike S6 of [PLAN.md](../../PLAN.md): which per-track parameters a slot can take, and their names. Marks as
digikit's (**[D]** read or measured once here, **[O]** open, **[V]** checked by a second agent; nothing here is
**[V]** yet).

## The parameter table **[D]**

`0x4020f59c`: 0x113 descriptors of 0x3c bytes. `+0` owner, `+4` value-array index, `+8 / +0xc / +0x10` minimum,
maximum and default (raw), `+0x28` long name, `+0x2c` owner name, `+0x30` short name. The per-track owners: machines
0–6 (Sample, Werp, Stretch, Repitch, Slice, MSlice; indices 25–34), filters 7–12 (35–37), filter envelope and
base-width 15 (38–48), Amp 13 (49–54, 58–61), track FX 17 (55–57, 64–68), Portamento 16 (62, 63), LFOs 28–30 (1–24).
Kit-wide owners (Chorus, Delay, Reverb, Master, Ext-in, CC) reuse the same indices for other values and are not
destinations.

## The list **[D]**

`src/mm_dests.c` has 28 destinations, in page order, with the table's short names and ranges: SRC TUNE (25,
`0x400–0x7c00`) and LEV (34); FLTR FREQ, RESO, ENV, DEL, ATK, DEC, SUS, REL, BASE, WDTH, KEY.T (36–43, 45, 46, 48,
KEY.T up to `0x6400`); AMP DEL, ATK, HOLD, DEC, SUS, REL, PAN, VOL (49–54, 58, 59); FX DEL, REV, CHR, BR, SRR, OVER (55–57,
64, 65, 67); PORT PTIM (62). The frame handler holds each value in its destination's range.

Left out: switches (play mode, sample slot and bank, envelope resets, routings, amp mode, portamento on), the filter
type, and indices 27 and 31–33, whose meaning and range change with the machine (Sample's STRT is Werp's SEG, a
switch, and Slice's SLICE). Measured in the scenario: slot 2 of macro C on LEV with depth −12 moves tracks 3 and 5 by
`0x23b` from `0x6400`; the kit's Sound buffers keep `0x6400`.

## Open **[O]**

- The machine-dependent source parameters, with names and ranges per track's machine (the machine byte is at
  `0x80003cd0 + 0x9a t`).
- The LFO parameters as destinations: whether a value added before the LFO tick reaches the LFOs.
- The Digitone II 1.12's table (the list there is still the provisional one).
