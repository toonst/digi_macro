| SPDX-License-Identifier: GPL-2.0-or-later
| The PERSONALIZE row's label (mm_glue.c): the firmware's MenuItem calls it as a std::function returning a
| std::string, built at a0 (the return slot), with d0 returning a0, as elekloader's examples/hooks-check-ar2 label.s.
        .text
        .globl  mm_row_label
        .align  2
mm_row_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        pea     -1(%a6)                 | the allocator, a byte on the frame
        pea     mm_row_name
        move.l  %a0, %d2
        move.l  %a0, -(%sp)
        jsr     STRING_CTOR
        lea     12(%sp), %sp
        move.l  %d2, %d0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts

mm_row_name:
        .asciz  "MACRO SETUP"
