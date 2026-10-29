| SPDX-License-Identifier: GPL-2.0-or-later
| Setups in the project on the Digitone II 1.12 (docs/findings/s7-project-storage.md): the kit record's save and
| load call mm_ser(record, kit) and mm_load(kit, record); the setup goes in bytes of the saved record that the
| firmware leaves unused. A pattern's kit has the pattern's number; kit is its RAM, which tells it.
|
| Save: the serializer 0x400ddfa8(record, kit, ...) at 0x400de0d4, before it writes the record's end mark (was:
| move.l #0xbacef00c,d1), with a2 the record and a3 the kit. Load: the loader 0x400dddb6(kit, record) at 0x400ddddc,
| once the record's version is checked and before its end mark is (the same instruction), with a3 the kit and a2 the
| record.
        .text
        .globl  mmx_ser, mmx_load
mmx_ser:
        move.l  %a3, -(%sp)
        move.l  %a2, -(%sp)
        jsr     mm_ser
        addq.l  #8, %sp
        move.l  #0xbacef00c, %d1        | the instruction the site replaced
        rts

mmx_load:
        move.l  %a2, -(%sp)
        move.l  %a3, -(%sp)
        jsr     mm_load
        addq.l  #8, %sp
        move.l  #0xbacef00c, %d1
        rts
