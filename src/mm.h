/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: the device-independent part (PLAN.md sections 3 and 4). Nothing here calls the firmware: the
 * elekloader glue (mm_glue.c) feeds key, encoder and tick events in and draws through a fill callback, and the
 * host tests (tests/) drive the same code. */
#ifndef MM_H
#define MM_H

#include <stdint.h>

#define MM_MACROS 8
#define MM_SLOTS 4
#define MM_TRACKS 16
#define MM_POS_MIN (-64) /* macro position and slot depth: -64 .. +63, 0 = centre / off (D5) */
#define MM_POS_MAX 63
#define MM_RAW_MAX 0x7f00 /* a parameter's raw range is 0 .. 0x7f00 (digikit finding 02) */
#define MM_COARSE 8       /* push + turn step (D3); [O] spike S1 measures the stock one */
#define MM_POPUP_TICKS 30 /* how long a turned knob shows its value: about 1 s of ev_tick */
#define MM_MAX_PAIRS (MM_MACROS * MM_SLOTS)

/* Key codes: the panel's, the same on both devices (digikit devices/digitakt-ii-leds.toml, digitone-ii-leds.toml). */
enum {
    MM_K_MOD = 6,
    MM_K_YES = 10,
    MM_K_UP = 11,
    MM_K_NO = 12,
    MM_K_LEFT = 13,
    MM_K_DOWN = 14,
    MM_K_RIGHT = 15,
    MM_K_TRK = 16,
    MM_K_FUNC = 17,
    MM_K_RECORD = 19,
    MM_K_PLAY = 20,
    MM_K_STOP = 21,
    MM_K_PAGE = 22,
    MM_K_PTN = 23,
    MM_K_SONG = 24,
    MM_K_TRIG1 = 25,  /* .. 40: TRIG 16 */
    MM_K_PUSH_A = 41, /* .. 48: encoder H pushed */
};
#define MM_ENC_LEVEL 9 /* [O] spike S1: the ev_enc id of LEVEL */

/* Key event flags (elekloader ev_key, ev+16). */
#define MM_F_PRESS 0x01
#define MM_F_FUNC 0x02
#define MM_F_REPEAT 0x08
#define MM_F_RELEASE 0x10

/* LED values the mode asks for: [O] digikit infers them (finding 19); spike S2 confirms. */
#define MM_LED_OFF 0x01
#define MM_LED_RED 0x11
#define MM_LED_BLUE 0x41

enum { MM_OFF = 0, MM_MACRO, MM_SETUP };

typedef struct {
    uint8_t dest;    /* value-array index; 0 = none */
    int8_t depth;    /* -64 .. +63, 0 = off */
    uint16_t tracks; /* bit t: track t + 1 */
} mm_slot;

typedef struct {
    int8_t pos; /* -64 .. +63, 0 = centre */
    mm_slot slot[MM_SLOTS];
} mm_macro;

/* The destinations a slot can take, in the order the encoder steps through them (W4, spike S6). */
typedef struct {
    const uint8_t *index;     /* value-array indices */
    const char *const *name;  /* up to 7 characters each; 0 entries (or a 0 table) are named by index */
    uint8_t count;
    uint8_t bound;            /* indices at or above this are never applied */
} mm_dests;

typedef struct mm {
    uint8_t state;        /* MM_OFF, MM_MACRO, MM_SETUP */
    uint8_t edit;         /* the macro SETUP shows */
    uint8_t sel;          /* the selected slot in SETUP (D12) */
    uint8_t popup;        /* knob + 1 whose value is shown, 0 = none */
    uint8_t popup_ticks;
    uint8_t dirty_screen; /* the frame must be recomposed */
    uint8_t dirty_mods;   /* the per-track lists must be rebuilt */
    uint8_t dirty_config; /* the configuration must be stored */
    uint8_t pushes;       /* bit i: push i held, its press taken */
    uint8_t func;         /* FUNC held */
    uint8_t mod_armed;    /* MOD held and its press went to the firmware: it can start a chord */
    uint8_t page_armed;   /* the same for PAGE */
    uint32_t taken[2];    /* bit c: the press of key c was taken, so its repeats and release are taken too */
    mm_macro m[MM_MACROS];
    const mm_dests *dests;
    int (*can_enter)(void); /* D9: 0 when a menu or dialog is open; a null pointer allows entry */
} mm_t;

/* Input, on the UI task. Each returns 1 when the mode takes the event, 0 when it goes on to the firmware. */
int mm_key(mm_t *s, int code, int flags);
int mm_enc(mm_t *s, int enc, int delta);
/* Once per ev_tick: returns 1 when the frame must be recomposed (and clears dirty_screen). */
int mm_tick(mm_t *s);

/* The per-track (destination, delta) lists the frame handler adds (PLAN.md 4.6). */
typedef struct {
    uint8_t n;
    uint8_t dest[MM_MAX_PAIRS];
    int16_t delta[MM_MAX_PAIRS];
} mm_track_mods;

typedef struct {
    uint16_t active; /* bit t: track t has pairs */
    mm_track_mods t[MM_TRACKS];
} mm_mods;

int32_t mm_slot_delta(int pos, int depth);
void mm_build(const mm_t *s, mm_mods *out);
/* Interrupt level: adds the deltas into the per-frame value arrays and saturates to 0 .. MM_RAW_MAX. */
void mm_apply(const mm_mods *mods, int16_t *values, int stride, int bound);

/* LEDs the mode holds: up to cap (key code, value) pairs; 0 when the mode is off (W1). */
int mm_leds(const mm_t *s, uint8_t *code, uint8_t *value, int cap);

/* The stored configuration (W3): a versioned blob, MM_BLOB_SIZE bytes. */
#define MM_BLOB_VERSION 1
#define MM_BLOB_SIZE (5 + MM_MACROS * MM_SLOTS * 4 + 2)
int mm_encode(const mm_t *s, uint8_t *out);         /* -> MM_BLOB_SIZE */
int mm_decode(mm_t *s, const uint8_t *in, int len); /* 0, or -1 (unknown version, bad size or check) */

/* Destinations. */
int mm_dest_pos(const mm_dests *d, int dest);       /* its place in the list, -1 for none or unknown */
void mm_dest_label(const mm_dests *d, int dest, char *buf); /* 8 bytes */

/* Screens: fill(ctx, x0, y0, x1, y1, colour) with y = 0 the top row, corners inclusive; colour 0 clears, 1 sets,
 * -1 inverts. */
typedef void mm_fill_fn(void *ctx, int x0, int y0, int x1, int y1, int colour);
typedef struct {
    mm_fill_fn *fill;
    void *ctx;
} mm_canvas;
void mm_draw(const mm_t *s, const mm_canvas *c);

#endif
