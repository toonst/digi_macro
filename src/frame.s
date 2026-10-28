| SPDX-License-Identifier: GPL-2.0-or-later
| ev_frame_params on the Digitakt II 1.17 and the Digitone II 1.12 (PLAN.md W2, docs/findings/s3-frame-values.md):
| each frame's per-track parameter values, before the stock modulators ride on them. Shaped like core's events
| (mods/core/core.s in elekloader), so it can move into the cores: the table ev_frame_params is the collection of
| mod.json.
|
| The site, in the frame interrupt (vector 191), was a jsr to MODS_STOCK, the per-track modulators, with the frame's
| value block pushed: (sp) = return, 4(sp) = the block, rebuilt from the smoother every frame (DT2: 0x4002e90c,
| modulators 0x400d9508, block 0x80005b50; DN2: 0x40027196, 0x400db390 with the mirror as a second argument, block
| 0x800068e4). Track t's values are int16 at block + 0x22 + 2 FRAME_STRIDE t, index j at + 2 j.
| Handlers: f(int16_t *values, int stride), interrupt level, d0-d1/a0-a1 free, everything else kept.
        .text
        .globl  mmx_frame
mmx_frame:
        move.l  %a2, -(%sp)
        lea     ev_frame_params, %a2
1:      move.l  (%a2)+, %d0
        beq.s   2f
        pea     FRAME_STRIDE            | the stride, in values: DT2 71, DN2 101
        move.l  12(%sp), %d1            | the block (after a2 and the stride)
        addi.l  #0x22, %d1
        move.l  %d1, -(%sp)
        movea.l %d0, %a0
        jsr     (%a0)
        addq.l  #8, %sp
        bra.s   1b
2:      movea.l (%sp)+, %a2
        jmp     MODS_STOCK              | the stock modulators, same stack
