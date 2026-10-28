/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode on elekloader's hook bus (core-dt2 1.0, core-dn2 0.2; PLAN.md 2.1 and 5): ev_key, ev_enc, ev_tick and
 * ev_draw. The only firmware routine it calls is Bitmap::fillRect, at FILLRECT_AT (mod.json cflags).
 *
 * Still to come (PLAN.md 6): storage (W3) and the check that no menu or dialog is open (D9); the configuration lives
 * in RAM until W3. */
#include "mm.h"

#ifndef FILLRECT_AT
#error "FILLRECT_AT: Bitmap::fillRect's address in the target OS (mod.json cflags)"
#endif

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

const mm_dests *mm_device_dests(void);

static mm_t mm_state;
static mm_mods mm_buf[2];
static uint8_t mm_back;
/* The lists the frame handler reads; written only on the UI task, by one pointer store. */
mm_mods *volatile mm_live;

static mm_t *state(void)
{
    if (!mm_state.dests)
        mm_state.dests = mm_device_dests();
    return &mm_state;
}

/* The firmware's frame has y = 0 at the bottom; the mode draws with y = 0 at the top. */
static void fw_fill(void *bmp, int x0, int y0, int x1, int y1, int colour)
{
    FILLRECT(bmp, x0, 63 - y1, x1, 63 - y0, colour);
}

int mm_ev_key(void *brain, void *ev)
{
    (void)brain;
    return mm_key(state(), *(int *)((char *)ev + 12), *(int *)((char *)ev + 16));
}

int mm_ev_enc(void *brain, void *ev)
{
    (void)brain;
    return mm_enc(state(), *(int *)((char *)ev + 12), *(int *)((char *)ev + 16));
}

void mm_ev_tick(void *ctrl)
{
    mm_t *s = state();
    if (s->dirty_mods) {
        mm_mods *b = &mm_buf[mm_back];
        mm_build(s, b);
        mm_live = b->active ? b : 0;
        mm_back ^= 1;
        s->dirty_mods = 0;
    }
    s->dirty_config = 0;            /* W3: stored here once storage exists; in RAM until then */
    if (mm_tick(s))
        *((unsigned char *)ctrl + 0x20) = 1;  /* recompose the frame */
    if (s->dirty_leds) {
        *((unsigned char *)ctrl + 0x21) = 1;  /* run the key-LED frame (docs/findings/s2-leds.md) */
        s->dirty_leds = 0;
    }
}

void mm_ev_draw(void *bmp, void *ctrl)
{
    mm_t *s = state();
    mm_canvas c = {fw_fill, bmp};
    (void)ctrl;
    if (s->state == MM_OFF)
        return;
    FILLRECT(bmp, 0, 0, 127, 63, 0);    /* the mode's screen replaces the stock one */
    mm_draw(s, &c);
}

/* W2: ev_frame_params (src/frame_dt2.s): each frame's per-track values, before the stock modulators. Interrupt
 * level: it only reads the lists the UI task published. */
void mm_ev_frame_params(int16_t *values, int stride)
{
    mm_mods *m = mm_live;
    if (m)
        mm_apply(m, values, stride, mm_state.dests->bound);
}

#ifdef LEDSET_AT
/* W1: ev_key_leds (src/leds_dt2.s). LedManager's setter (mgr, logical id, layer, value): layer 2 lights the LED with
 * the value, layer 1 turns it off, and either keeps the views' later sets out (docs/findings/s2-leds.md). The logical
 * id of key code c is MOD 25 and TRIG n 25 + n (digikit finding 19). */
typedef void (*ledset_t)(void *mgr, int logical, int layer, int value);
void mm_ev_key_leds(void *mgr)
{
    uint8_t code[1 + MM_TRACKS], value[1 + MM_TRACKS];
    int n = mm_leds(&mm_state, code, value, 1 + MM_TRACKS);
    for (int i = 0; i < n; i++)
        ((ledset_t)LEDSET_AT)(mgr, code[i] == MM_K_MOD ? 25 : code[i] + 1, value[i] == MM_LED_OFF ? 1 : 2, value[i]);
}
#endif
