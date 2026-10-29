| SPDX-License-Identifier: GPL-2.0-or-later
| The kits feature's calls from the firmware (docs/findings/s7-project-storage.md), on the Digitakt II 1.17 and the
| Digitone II 1.12 alike: the save and load of a kit record and of the project's header, and the copies of a kit in
| RAM. The C side is src/mm_kits.c.
|
| Kit records: the serializer (DT2 0x400dbdca, DN2 0x400ddfa8)(record, kit, ...) writes the record's end mark, and the
| loader (DT2 0x400db898, DN2 0x400dddb6) checks it, with move.l #0xbacef00c,d1, the instruction the sites replace:
|   save at DT2 0x400dbee4, DN2 0x400de0d4, with a2 the record and a3 the kit's RAM;
|   load at DT2 0x400db8be, DN2 0x400ddddc, after the version check, a2 and a3 the kit's RAM and the record in the
|   order the device has (mm_kits.c: KIT_LOAD_RAM_A3).
| The project's header: the save (DT2 0x400df66c, DN2 0x400e15f8) writes its mark with move.l #0xbeefbace,(a3), at
| DT2 0x400df932, DN2 0x400e18be, and the load (DT2 0x400df95a, DN2 0x400e18e6) checks it with move.l #0xbeefbace,d0,
| at DT2 0x400df99c, DN2 0x400e1928; a3 the file's buffer in both. The save goes on with d0 set, so it is kept.
| Copies: memcpy (DT2 0x40136388, DN2 0x40134770) begins with three loads of its arguments (12 bytes, the site); a
| copy of a kit's size also calls mm_kit_copied(dst, src).
        .text
        .globl  mmx_kit_save, mmx_kit_load, mmx_proj_save, mmx_proj_load, mmx_memcpy
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

mmx_proj_save:
        move.l  #0xbeefbace, (%a3)      | the instruction the site replaced
        lea     -16(%sp), %sp
        movem.l %d0-%d1/%a0-%a1, (%sp)
        move.l  %a3, -(%sp)
        jsr     mm_proj_save
        addq.l  #4, %sp
        movem.l (%sp), %d0-%d1/%a0-%a1
        lea     16(%sp), %sp
        rts

mmx_proj_load:
        move.l  %a3, -(%sp)
        jsr     mm_proj_load
        addq.l  #4, %sp
        move.l  #0xbeefbace, %d0
        rts

| memcpy(dst, src, n), in place of its first 12 bytes: those loads, then on into its body (MEMCPY + 12)
mmx_memcpy:
        move.l  12(%sp), %d0
        cmpi.l  #KIT_SIZE, %d0
        beq.s   1f
2:      movea.l 4(%sp), %a1
        movea.l 8(%sp), %a0
        move.l  12(%sp), %d0
        jmp     MEMCPY + 12
1:      move.l  12(%sp), -(%sp)         | a kit: the copy, then the call
        move.l  12(%sp), -(%sp)
        move.l  12(%sp), -(%sp)
        bsr.s   2b
        lea     12(%sp), %sp
        move.l  %d0, -(%sp)             | memcpy's result
        move.l  12(%sp), -(%sp)         | src
        move.l  12(%sp), -(%sp)         | dst
        jsr     mm_kit_copied
        addq.l  #8, %sp
        move.l  (%sp)+, %d0
        rts
