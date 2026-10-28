| SPDX-License-Identifier: GPL-2.0-or-later
| ev_key_leds on the Digitakt II 1.17 and the Digitone II 1.12 (PLAN.md W1, docs/findings/s2-leds.md): the key-LED
| frame, after the frame clears the LEDs and before the stock views set theirs. The first setter of an LED wins
| (LedManager's setter refuses an LED whose layer is set), so what a handler sets stays over the views'. The same
| event and prototype as elekloader's core-dn1 3.3, in the shape of its core events, so it can move into the cores.
|
| The site (DT2 0x400311a2 in the LED frame 0x4003117c; DN2 0x4002c87a in 0x4002c854) was: jsr (pc) LEDS_CLEAR
| (clear every LED's layer and value; the LedManager pushed before it), then move.l d2,-(sp) (the view controller,
| for the call that lets the views set theirs). Handlers: f(void *mgr), on the UI task.
        .text
        .globl  mmx_leds
mmx_leds:
        move.l  4(%sp), -(%sp)          | mgr
        jsr     LEDS_CLEAR
        addq.l  #4, %sp
        move.l  %a2, -(%sp)
        lea     ev_key_leds, %a2
1:      move.l  (%a2)+, %d0
        beq.s   2f
        move.l  8(%sp), -(%sp)          | mgr (after the saved a2 and the return)
        movea.l %d0, %a0
        jsr     (%a0)
        addq.l  #4, %sp
        bra.s   1b
2:      movea.l (%sp)+, %a2
        movea.l (%sp)+, %a0             | the return
        move.l  %d2, -(%sp)             | the push the site displaced
        jmp     (%a0)
