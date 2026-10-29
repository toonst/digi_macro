| SPDX-License-Identifier: GPL-2.0-or-later
| Setups in the kits (docs/findings/s7-project-storage.md), on the Digitakt II 1.17 and the Digitone II 1.12 alike:
| the firmware's save and load of a kit record call mm_kit_save and mm_kit_load, which put the setup in bytes after
| the record's end mark.
|
| Both routines check or write that mark with move.l #0xbacef00c,d1, the instruction the sites replace:
|   save: the serializer (DT2 0x400dbdca, DN2 0x400ddfa8)(record, kit, ...) before it writes the mark, at DT2
|         0x400dbee4, DN2 0x400de0d4; a2 the record, a3 the kit's RAM.
|   load: the loader (DT2 0x400db898, DN2 0x400dddb6), after it checks the record's version and before its mark, at
|         DT2 0x400db8be, DN2 0x400ddddc; a2 and a3 the kit's RAM and the record, in the order the device has
|         (mm_glue.c: KIT_LOAD_RAM_A3).
        .text
        .globl  mmx_kit_save, mmx_kit_load
mmx_kit_save:
        move.l  %a3, -(%sp)
        move.l  %a2, -(%sp)
        jsr     mm_kit_save
        addq.l  #8, %sp
        move.l  #0xbacef00c, %d1        | the instruction the site replaced
        rts

mmx_kit_load:
        move.l  %a3, -(%sp)
        move.l  %a2, -(%sp)
        jsr     mm_kit_load
        addq.l  #8, %sp
        move.l  #0xbacef00c, %d1
        rts
