# S5: where the configuration is kept (Digitakt II 1.17, Digitone II 1.12)

Spike S5 of [PLAN.md](../../PLAN.md): keep the macros' configuration across restarts, globally for now (D4). Marks as
digikit's (**[D]** read or measured once here, **[O]** open, **[V]** checked by a second agent; nothing here is
**[V]** yet).

## Result **[D]**

Two card sectors the stock OS never writes, `0x2DF0E00` and `0x2DF0E01`, through the firmware's own eMMC block read
and write. They lie in the gap between the end of the +Drive (`0x2DF0180`) and digikit's store (`0x2DF1000`; digikit
finding 59: `0x2DF0180..0x2E19FFF` is free and never written on DT2 1.17), so they touch neither the firmware's data
nor digikit's.

Records in the firmware's own files were looked at first: the kit's FX storage record has 28 spare halfwords (digikit
finding 61), too few for the 135-byte configuration; the firmware's global settings record is not mapped yet. Both stay
candidates for the per-pattern and per-project scopes of D4.

## The Digitone II 1.12 **[D]**

Sectors `0x7FF000` and `0x7FF001`, with the block read `0x4012c87a` and write `0x4012ca60` (digikit's port file). Stock
writes nothing at or above `0x458000` on the Digitone II (a FORMAT trims up to `0x457FFF`), and digikit's store
starts at `0x800000` (finding 59). Unlike that store, these two are below `0x800000`, where a crafted Data-API
request could reach them; a record it damaged fails its check and is ignored. The scenario's restart part passes the
same way on the Digitone II.

## The routines on 1.17 **[D]**

From digikit's port file `devices/ports/dt2-1.17.toml`, checked in the disassembly:

| Address | What |
| --- | --- |
| `0x4012e1b6` | `int block_read(u32 sector, u32 bytes, void *buf)`, 0 = ok; in parts of at most 64 KiB |
| `0x4012e39c` | `int block_write(u32 sector, u32 bytes, const void *buf)`, 0 = ok; it takes the ESDHC mutex itself (digikit's declaration) |

## The record **[D]**

512 bytes: `"MMCF"`, a big-endian sequence number, then the configuration blob of `src/mm_store.c` (`"MM"`, its version,
8 × 4 slots, the macros' names, a CRC-16). Version 2 has 16-bit depths and the names; a version 1 record loads with
each depth step as 2.00 (the same offset) and no names. At the first `ev_tick` the mod reads both sectors and loads the valid copy with the higher
sequence number (`mm_record_pick`); a write goes to the other copy with the number one higher, so a power cut during a
write leaves the previous configuration. It writes when the setup screen closes, not on every turn: one sector per
visit. Positions are not kept (D14).

## Measured in the emulator **[D]**

`tests/emu/scenario.py`, with the zero-DSP runner and an empty +Drive card: after the setup of macro C the guest wrote
sector `0x2DF0E01` (sequence 1) and nothing else above the +Drive; a cold boot with only that sector on a fresh card
starts with every macro at the centre (the frame values equal the mirror), and turning C to −6 moves tracks 3 and 5 by
the stored slot's delta and track 1 not at all.

## Open **[O]**

- That the gap stays unwritten on a used unit, and the unit's real card capacity (digikit finding 59 assumes the
  whitelist's).
- A DIGIKIT-style format of the gap: nothing here erases it; a record that is not ours (no `"MMCF"`, or a bad CRC) is
  ignored, and the first write then puts ours in the other copy.
- The per-pattern and per-project scopes (D4 later): the firmware's pattern, kit and project records, and a setting to
  choose.
