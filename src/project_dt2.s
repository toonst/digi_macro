| SPDX-License-Identifier: GPL-2.0-or-later
| Setups in the project on the Digitakt II 1.17 (docs/findings/s7-project-storage.md): the pattern record's save and
| load call mm_ser(record, pattern) and mm_load(pattern, record); the setup goes in bytes of the saved record that
| the firmware leaves unused. pattern is the pattern's RAM, which tells its number.
|
| Save: the serializer 0x400dc894(record, pattern, ...) copies the pattern's last 0x28 bytes with memcpy at
| 0x400dc8cc (record + 0x15aa4, pattern + 0x1db60, 0x28); the site calls mmx_ser instead, with d2 the record and
| d3 the pattern. Load: the loader 0x400dc702(pattern, record, number), once the record is in, at 0x400dc7e6 (was:
| move.l #0x1db88,d0), with a2 the pattern and d2 the record + 0x10a04.
        .text
        .globl  mmx_ser, mmx_load
mmx_ser:
        move.l  12(%sp), -(%sp)         | the memcpy it replaces: n, from, to
        move.l  12(%sp), -(%sp)
        move.l  12(%sp), -(%sp)
        jsr     MEMCPY
        lea     12(%sp), %sp
        move.l  %d3, -(%sp)
        move.l  %d2, -(%sp)
        jsr     mm_ser
        addq.l  #8, %sp
        rts

mmx_load:
        move.l  %d2, %d0
        subi.l  #0x10a04, %d0
        move.l  %d0, -(%sp)
        move.l  %a2, -(%sp)
        jsr     mm_load
        addq.l  #8, %sp
        move.l  #0x1db88, %d0           | the instruction the site replaced
        rts
