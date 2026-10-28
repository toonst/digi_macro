/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode on elekloader's hook bus (core-dt2 1.0, core-dn2 0.2; PLAN.md 2.1 and 5): ev_key, ev_enc, ev_tick and
 * ev_draw. It draws with the firmware's Bitmap::fillRect (FILLRECT_AT) and, where the device has it, its text routine
 * (TEXT_AT, with the font object at TEXT_FONT).
 *
 * The device's addresses come from mod.json's cflags; docs/findings has what each is. */
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
/* The firmware's Brain, as ev_key last handed it over (its view controller is at +0x40). */
void *mm_brain;

#ifdef CARD_WRITE_AT
/* W3 (docs/findings/s5-storage.md): the configuration in two card sectors the stock OS never writes, between the end
 * of the +Drive and digikit's store; the firmware's eMMC block read and write, int f(sector, bytes, buf), 0 = ok. */
typedef int (*block_fn)(uint32_t sector, uint32_t bytes, void *buf);
static uint8_t mm_sector[MM_RECORD_SIZE] __attribute__((aligned(16)));
static uint32_t mm_seq;
static uint8_t mm_copy, mm_loaded;

static void card_load(mm_t *s)
{
    static uint8_t other[MM_RECORD_SIZE] __attribute__((aligned(16)));
    uint32_t seq = 0;
    int pick;
    mm_loaded = 1;
    if (((block_fn)CARD_READ_AT)(CARD_SECTOR, MM_RECORD_SIZE, mm_sector) ||
        ((block_fn)CARD_READ_AT)(CARD_SECTOR + 1, MM_RECORD_SIZE, other))
        return;                     /* no card: the configuration stays in RAM */
    pick = mm_record_pick(s, mm_sector, other, &seq);
    if (pick < 0)
        return;
    mm_decode(s, (pick ? other : mm_sector) + 8, MM_BLOB_SIZE);
    mm_seq = seq;
    mm_copy = (uint8_t)pick;
}

static void card_store(mm_t *s)
{
    mm_record_encode(s, mm_seq + 1, mm_sector);
    if (((block_fn)CARD_WRITE_AT)(CARD_SECTOR + (mm_copy ^ 1), MM_RECORD_SIZE, mm_sector) == 0) {
        mm_seq++;
        mm_copy ^= 1;               /* the next write goes to the other copy */
    }
}
#endif

#ifdef VIEW_TOP_BASE
/* D9 (docs/findings/s1-input.md): macro mode starts only when no menu, dialog or popup is open, that is when the top
 * of the view controller's stack (Brain + 0x40; list sentinel at + 0x14, its prev the top node, the view at node + 8)
 * is the view that tops the stack on the main screen and the parameter pages. */
static int fw_can_enter(void)
{
    char *sentinel = (char *)mm_brain + 0x40 + 0x14;
    char *top;
    if (!mm_brain)
        return 0;
    top = *(char **)(sentinel + 4);
    return top != sentinel && **(uint32_t **)(top + 8) == VIEW_TOP_BASE;
}
#endif

static mm_t *state(void)
{
    if (!mm_state.dests) {
        mm_state.dests = mm_device_dests();
#ifdef VIEW_TOP_BASE
        mm_state.can_enter = fw_can_enter;
#endif
    }
    return &mm_state;
}

/* The firmware's frame has y = 0 at the bottom; the mode draws with y = 0 at the top. */
static void fw_fill(void *bmp, int x0, int y0, int x1, int y1, int colour)
{
    FILLRECT(bmp, x0, 63 - y1, x1, 63 - y0, colour);
}

#ifdef TEXT_AT
/* The firmware's printf-style text routine (bitmap, font, x, y, flags, fmt, ...): y is the bottom row of the text,
 * counted from the bottom; flags bit 1 centres on x (docs/findings/s4-screens.md). */
typedef int (*text_t)(void *bmp, void *font, int x, int y, unsigned flags, const char *fmt, ...);
static void fw_text(void *bmp, int x, int ytop, int centre, const char *str)
{
    ((text_t)TEXT_AT)(bmp, (void *)TEXT_FONT, x, 59 - ytop, centre ? 2 : 0, "%s", str);
}
#endif

int mm_ev_key(void *brain, void *ev)
{
    mm_brain = brain;
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
#ifdef CARD_WRITE_AT
    if (!mm_loaded)
        card_load(s);
    if (s->dirty_config && s->state != MM_SETUP) {  /* once per visit of the setup screen, not per turn */
        card_store(s);
        s->dirty_config = 0;
    }
#else
    s->dirty_config = 0;
#endif
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
#ifdef TEXT_AT
    mm_canvas c = {fw_fill, bmp, fw_text};
#else
    mm_canvas c = {fw_fill, bmp, 0};
#endif
    (void)ctrl;
    if (s->state == MM_OFF)
        return;
    /* the macro screen keeps the stock header (pattern, name, tempo); the setup screen has its own title */
    FILLRECT(bmp, 0, 0, 127, s->state == MM_MACRO ? 63 - MM_HEADER : 63, 0);
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
