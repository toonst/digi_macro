# S7: macro setups in the project's data (Digitakt II 1.17, Digitone II 1.12)

D4: the setups per pattern or per project, kept in the project itself, so they go wherever it goes. Marks as digikit's
(**[D]** read or measured once here, **[O]** open). Measured with `tests/emu/zero_patched.rs` (`poke` writes guest
memory, `dump` saves a range of it) and `tests/emu/project.py`.

## The project in RAM **[D]**

The loaded project is one object: DT2 at `0x41776c9c`, DN2 at `0x41219970` (the address the firmware's autosave
passes to the save routine). Its 128 patterns follow at + 0x1c (DT2 stride `0x1db8c`, DN2 `0x1de6e`); its kits at
+ `0xedc61c` (DT2, 128 of `0x5574`) and + `0xef371c` (DN2, 129 of `0x5d71`; kit n is pattern n's). The firmware copies
whole pattern records for copy and undo (digikit finding 09).

The pattern records' bytes the save leaves out are not free: on DT2, + `0x1cef0` .. `0x1db60` (the part after the
serialized `80 x 258` block) holds `0xff` and a 16 x 128 array the code at `0x400d6cd6` reads, and pokes into it
hung the UI. So the mod keeps the setups in its own RAM and only puts them in the saved file.

## The saved project **[D]**

A project file (DT2 and DN2 alike) starts `0xbeefbace`, version 5, the name; 128 pattern records of `0x15c00` from
offset 512; then the kits from `0xae0200`; ends `0xbacef00c` at `0xdd9600` (DT2). The save routine (DT2 `0x400df66c`,
DN2 `0x400e15f8`) serializes into a buffer and writes it; the load routine (DT2 `0x400df95a`, DN2 `0x400e18e6`) checks
the marks and deserializes. The DT2 buffer at `0x4099d698` holds the boot-loaded file, but SAVE PROJECT AS writes from
another one: bytes put into the first did not reach the card.

Per record, the bytes the serializer writes nothing to (zero in every save seen):

| | Record | Serializer, loader | Written | Unused |
| --- | --- | --- | --- | --- |
| DT2 pattern | `0x15c00` | `0x400dc894`, `0x400dc702` | version 4, 16 tracks x `0x4a0`, `0xc000`, `80 x 258`, 0x28 raw (RAM + `0x1db60`) to `0x15acc` | `0x15acc` .. `0x15c00` (308) |
| DT2 kit | `0x5800` | `0x400dbdca`, `0x400db898` | to the end mark at `0x573a` | `0x573e` .. `0x5800` (194) |
| DN2 pattern | `0x15c00` | `0x400de90c`, `0x400de77a` | as DT2's, but 16 x `0x4a3` and 0x12a raw (RAM + `0x1dd40`) to `0x15bfe` | 2 bytes |
| DN2 kit | `0x2a00` | `0x400ddfa8`, `0x400dddb6` | to the end mark at `0x28ab` | `0x28af` .. `0x2a00` (337) |

The DT2 pattern record's loader version converters (`0x400de220` from version 3, `0x400de420` from 2) show its raw
tail moving from `0x15a94` to `0x15aa4` between versions: a later OS may grow into these bytes, and the mod's record
carries its own mark and CRC so it never takes them for a setup.

## The mod's records **[D]**

A setup is 205 bytes: `MS`, flags (bit 0: written per project), the 8 x 4 slots and the names as in the global blob
(s5), a CRC-16. One goes into each pattern's saved record on DT2 (at `0x15acc`) and each kit's on DN2 (at `0x28b0`).
Four sites call the mod (`src/project_dt2.s`, `src/project_dn2.s`):

| | Save | Load |
| --- | --- | --- |
| DT2 | `0x400dc8cc`, the serializer's memcpy of the raw tail (d2 the record, d3 the pattern) | `0x400dc7e6` (`move.l #0x1db88,d0`), after the record is in (a2 the pattern, d2 the record + `0x10a04`) |
| DN2 | `0x400de0d4` (`move.l #0xbacef00c,d1`), before the end mark (a2 the record, a3 the kit) | `0x400ddddc`, the same instruction, after the version check (a3 the kit, a2 the record) |

The RAM address tells the pattern number; a record from anywhere else gets no setup. Besides the project save and
load, the same routines serve the working project's autosave (DT2 `0x400c0e08`) and other callers of the pattern
serializer (DT2 `0x4003e856`, `0x4003f516`, `0x40047f74`, `0x400e2dd0`, `0x400e301c`; what each is was not traced).

The pattern on is `*(*PATTERN_OBJ + 0x138)`, with `PATTERN_OBJ` DT2 `0x44f370a0`, DN2 `0x446488d0` (the pointer the
getter DT2 `0x40198b7e` returns): the only word that went 0, 1, 2 on DT2 as A01, A02 and A03 came on, and the same
object shape on DN2.

In `tests/emu/project.py` (14 checks on each, all passing): the PERSONALIZE row toggles with YES, LEFT and RIGHT; per
pattern, A02 starts as a copy of A01's setup and each keeps its own; SAVE PROJECT AS to slot 001 writes both into
the file and none for A03; LOAD PROJECT of slot 001, with the mod's copies cleared, brings both back and A01's comes
on. Between the end marks and the mod's bytes, nothing else changed.

## PERSONALIZE **[D]**

The row is a stock `MenuItem` from elekloader's `core_additem`, drawn as LED INTENSITY is (DT2 `0x4009c722`: the
text routine DT2 `0x40113fd4`, DN2 `0x40115948`, font DT2 `0x402a9be0`, DN2 `0x40294710`, at the row's x + 19,
y + 2, right-aligned). Its change callback gets (storage, item, delta) and redraws with `View::invalidate(menu +
0x38)` (DT2 `0x4011b126`, DN2 `0x4011ca9a`). LEFT and RIGHT call it; the LEVEL encoder does not, in this menu.

## Open **[O]**

- Pattern copy and paste inside the project copy the RAM records, not the mod's setups: a pasted pattern keeps the
  setup its slot had.
- Transfer: its project backups carry the file as saved, so the setups go with them; not tried. Its single-pattern
  sends go through the same serializer.
- Not tried on a unit.
