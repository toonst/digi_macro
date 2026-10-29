# S7: macro setups in the project's data (Digitakt II 1.17, Digitone II 1.12)

D4: the setups per kit or per project, kept in the project's kit records, so they go wherever a kit or the project
goes. Marks as digikit's
(**[D]** read or measured once here, **[O]** open). Measured with `tests/emu/zero_patched.rs` (`poke` writes guest
memory, `dump` saves a range of it) and `tests/emu/project.py`.

## The project in RAM **[D]**

The loaded project is one object: DT2 at `0x41776c9c`, DN2 at `0x41219970` (the address the firmware's autosave
passes to the save routine). Its 128 patterns follow at + 0x1c (DT2 stride `0x1db8c`, DN2 `0x1de6e`); its kits at
+ `0xedc61c` (DT2: `0x426532b8`, 128 of `0x5574`) and + `0xef371c` (DN2: `0x4210d08c`, 129 of `0x5d71`). Each
pattern has its own kit, kit n (digikit finding 09); both OSes also have a kit library (KIT MANAGER: SAVE KIT, LOAD KIT
TO PATTERN, PASTE KIT, RELOAD KIT FROM SAVED). The firmware copies whole pattern records for copy and undo.

The RAM records have no room of their own: a DT2 kit's `0x5574` bytes are all serialized, and the pattern records'
bytes the save leaves out are in use (on DT2, + `0x1cef0` .. `0x1db60` holds `0xff` and a 16 x 128 array the code at
`0x400d6cd6` reads; pokes into it hung the UI). So the mod keeps the setups in its own RAM and puts them only in the
saved records.

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
tail moving from `0x15a94` to `0x15aa4` between versions: a later OS may grow into unused bytes, and the mod's record
carries its own mark and CRC so it never takes other bytes for a setup.

## The mod's records **[D]**

The setups live in the kit records, on both devices: right after the end mark, where the DT2 has 194 bytes and the
DN2 337. A setup is 194 bytes: the mark `0xd1`, then a bit stream (high bit first) with per slot the destination
(7 bits), depth (16) and tracks (16), then the 40 name characters (7 bits each), then a CRC-16 of all before.

The kit serializer and loader have the same shape on both OSes, and both check or write the end mark with
`move.l #0xbacef00c,d1`; that instruction is the site on each (`src/kit.s`):

| | Save (a2 the record, a3 the kit's RAM) | Load |
| --- | --- | --- |
| DT2 | `0x400dbee4` in `0x400dbdca` | `0x400db8be` in `0x400db898`, after the version check (a2 the kit's RAM, a3 the record) |
| DN2 | `0x400de0d4` in `0x400ddfa8` | `0x400ddddc` in `0x400dddb6`, the same (a3 the kit's RAM, a2 the record) |

The kit's RAM address gives its number; a kit outside the project's table gets no setup. Besides the project save
and load, the serializer has nine other callers on each OS (DT2 `0x40035292`, `0x40035532`, `0x40035760`,
`0x40035790`, `0x40043834`, `0x400d3bae`, `0x400e2ee8`, `0x400e3034`, `0x401332b8`) and the loader four (DT2
`0x40044612`, `0x400485be`, `0x40048978`, `0x400e1570`). `0x40044612` loads into the table (kit n of the project
object, from a copy at `0x409908e4`: the reload); `0x40043834` saves a table kit into a buffer at `0x40996ce4`. The others pass kit
pointers that were not traced.

The kit on is the pattern on's: `*(*PATTERN_OBJ + 0x138)`, with `PATTERN_OBJ` DT2 `0x44f370a0`, DN2 `0x446488d0`
(the pointer the getter DT2 `0x40198b7e` returns). It was the only word that went 0, 1, 2 on DT2 as A01, A02 and A03
came on, and DN2's object has the same shape.

A project load brings all 128 kits and a kit manager load one, so the mod counts the kit records loaded in a burst
(up to a tick without loads). Per kit, the kit on takes its setup again after any burst. Per project, a burst of
128 or more is a project load, and the project takes the setup of the kit on. When saved per project, every kit
carries the project's setup.

In `tests/emu/project.py` (16 checks on each, all passing):
- The PERSONALIZE row toggles with YES, LEFT and RIGHT.
- Per kit, A02's kit starts as a copy of A01's setup, and each keeps its own.
- SAVE PROJECT AS to slot 001 writes both into their kit records, and none for kit 3.
- Per project, LOAD PROJECT of slot 001 (with the mod's copies cleared) brings both back, the project takes A01's
  kit's setup, and changing the pattern then changes nothing.

Apart from the mod's bytes, nothing after the end marks changed.

## PERSONALIZE **[D]**

The row is a stock `MenuItem` from elekloader's `core_additem`, drawn as LED INTENSITY is (DT2 `0x4009c722`: the
text routine DT2 `0x40113fd4`, DN2 `0x40115948`, font DT2 `0x402a9be0`, DN2 `0x40294710`, at the row's x + 19,
y + 2, right-aligned). Its change callback gets (storage, item, delta) and redraws with `View::invalidate(menu +
0x38)` (DT2 `0x4011b126`, DN2 `0x4011ca9a`). LEFT and RIGHT call it; the LEVEL encoder does not, in this menu.

## Open **[O]**

- The kit manager (SAVE KIT, LOAD KIT TO PATTERN, RELOAD KIT FROM SAVED) was not driven in the emulator: which of the
  callers above it uses, and whether they pass the project's table kits, is not known for all of them.
- Copies inside RAM (PASTE KIT, pattern copy and paste) copy the firmware's records, not the mod's setups: the target
  keeps the setup its kit had.
- Transfer: its project backups carry the file as saved, so the setups should go with them; not tried.
- Not tried on a unit.
