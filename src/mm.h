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
#define MM_POS_MIN (-64) /* macro position: -64 .. +63, 0 = centre (D5) */
#define MM_POS_MAX 63
/* Slot depth as the stock modulation setups have it: -128.00 .. +127.99 in 1/256 steps, 0 = off; at full depth and
 * full turn the offset spans the parameter range. */
#define MM_DEPTH_MIN (-32768)
#define MM_DEPTH_MAX 32767
#define MM_DEPTH_ONE 256  /* 1.00 */
#define MM_NAME_LEN 5     /* a macro's name, as a MIDI machine parameter's */
#define MM_HOLD_TICKS 15  /* FUNC + push held this long (about 0.5 s of ev_tick) names the macro */
#define MM_RAW_MAX 0x7f00 /* a parameter's raw range is 0 .. 0x7f00 (digikit finding 02) */
#define MM_COARSE 8       /* push + turn step (D3) without the firmware's encoder filter (the host tests) */
#define MM_POPUP_TICKS 30 /* how long a turned knob shows its value: about 1 s of ev_tick */
#define MM_MAX_PAIRS (MM_MACROS * MM_SLOTS)

/* Key codes: the panel's, the same on both devices (digikit devices/digitakt-ii-leds.toml, digitone-ii-leds.toml). */
enum {
    MM_K_TRIG = 1,    /* the page keys: TRIG, SRC, FLTR, AMP, FX (1 .. 5) and MOD */
    MM_K_FX = 5,
    MM_K_MOD = 6,
    MM_K_YES = 10,
    MM_K_UP = 11,
    MM_K_NO = 12,
    MM_K_LEFT = 13,
    MM_K_DOWN = 14,
    MM_K_RIGHT = 15,
    MM_K_TRK = 16,    /* held with a trig key: selects that track */
    MM_K_FUNC = 17,
    MM_K_RECORD = 19,
    MM_K_PLAY = 20,
    MM_K_STOP = 21,
    MM_K_PAGE = 22,
    MM_K_PTN = 23,
    MM_K_SONG = 24,
    MM_K_TRIG1 = 25,  /* .. 40: TRIG 16 */
    MM_K_PUSH_A = 41, /* .. 48: encoder H pushed */
    MM_K_LEVEL = 49,  /* LEVEL/DATA pushed */
};
#define MM_ENC_LEVEL 9 /* [O] spike S1: the ev_enc id of LEVEL/DATA; it moves every macro at once */

/* Key event flags (elekloader ev_key, ev+16). */
#define MM_F_PRESS 0x01
#define MM_F_FUNC 0x02
#define MM_F_REPEAT 0x08
#define MM_F_RELEASE 0x10

/* LED values the mode asks for (docs/findings/s2-leds.md): the high nibble is a palette index, seen on a Digitakt II:
 * 1 red, 2 green, 3 deep blue, 4 yellow, 5 light blue, 6 purple, 7 white, 8 orange; 0x01 is off. */
#define MM_LED_OFF 0x01
#define MM_LED_RED 0x11
#define MM_LED_BLUE 0x31

enum { MM_OFF = 0, MM_MACRO, MM_SETUP };

typedef struct {
    uint8_t dest;    /* value-array index; 0 = none */
    int16_t depth;   /* MM_DEPTH_MIN .. MM_DEPTH_MAX, 1/256 units, 0 = off */
    uint16_t tracks; /* bit t: track t + 1 */
} mm_slot;

typedef struct {
    int8_t pos; /* -64 .. +63, 0 = centre */
    mm_slot slot[MM_SLOTS];
    char name[MM_NAME_LEN + 1]; /* "" until named */
} mm_macro;

/* The destinations a slot can take, in the order the encoder steps through them (W4, spike S6). */
enum { MM_G_NONE = 0, MM_G_SRC, MM_G_FLTR, MM_G_AMP, MM_G_FX, MM_G_PORT, MM_G_COUNT };
typedef struct {
    uint8_t index;    /* value-array index */
    uint8_t group;    /* MM_G_*: the page it is on, the setup screen's column heading */
    uint16_t lo, hi;  /* the raw range the frame value is held in; hi 0 means 0 .. MM_RAW_MAX */
    const char *name; /* the firmware's short name, up to 5 characters; 0 names it by index */
    const char *full; /* the firmware's long name, for the destination list; 0: the short one */
} mm_dest;

typedef struct {
    const mm_dest *d;
    uint8_t count;
    uint8_t bound;    /* indices at or above this are never applied */
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
    uint8_t dirty_leds;   /* the LEDs the mode holds changed */
    uint8_t pushes;       /* bit i: push i held, its press taken */
    uint8_t func;         /* FUNC held */
    uint8_t mod_armed;    /* MOD held and its press went to the firmware: it can start a chord */
    uint8_t page_armed;   /* the same for PAGE */
    uint32_t taken[2];    /* bit c: the press of key c was taken, so its repeats and release are taken too */
    mm_macro m[MM_MACROS];
    const mm_dests *dests;
    int (*can_enter)(void); /* D9: 0 when a menu or dialog is open; a null pointer allows entry */
    /* The firmware's key state, 1 while key code c is down. With it, FUNC, the pushes and the entry chord are read
     * from the panel, not from releases, which the firmware does not always pass on (FUNC's, after FUNC + NO). */
    int (*key_down)(int code);
    uint8_t enc_scaled;   /* encoder deltas arrive in steps, push already applied (the firmware's encoder filter) */
    uint8_t list;         /* SETUP: slot + 1 whose destination list is open, 0 = none */
    uint8_t list_top;     /* the list's first visible line */
    uint8_t trk;          /* TRK held (without key_down) */
    uint8_t announce;     /* the mode was entered: the glue shows its title in the header, as a page's */
    uint8_t name_arm;     /* macro + 1 of a FUNC + push still held: short, its setup; held, its name */
    uint8_t name_ticks;
    uint8_t name_req;     /* macro + 1 to name: the glue opens the firmware's naming screen */
} mm_t;

#define MM_LIST_ROWS 7    /* the destination list's visible lines */
/* A fresh state: every macro at the centre, every slot on all tracks. */
void mm_init(mm_t *s, const mm_dests *dests);

/* Input, on the UI task. Each returns 1 when the mode takes the event, 0 when it goes on to the firmware. */
int mm_key(mm_t *s, int code, int flags);
int mm_enc(mm_t *s, int enc, int delta);
/* Once per ev_tick: returns 1 when the frame must be recomposed (and clears dirty_screen). dirty_leds is the glue's. */
int mm_tick(mm_t *s);

/* The per-track (destination, delta) lists the frame handler adds (PLAN.md 4.6). */
typedef struct {
    uint8_t n;
    uint8_t dest[MM_MAX_PAIRS];
    int16_t delta[MM_MAX_PAIRS];
    uint16_t lo[MM_MAX_PAIRS], hi[MM_MAX_PAIRS]; /* the destination's range */
} mm_track_mods;

typedef struct {
    uint16_t active; /* bit t: track t has pairs */
    mm_track_mods t[MM_TRACKS];
} mm_mods;

int32_t mm_slot_delta(int pos, int depth);
void mm_build(const mm_t *s, mm_mods *out);
/* Interrupt level: adds the deltas into the per-frame value arrays and saturates to each destination's range. */
void mm_apply(const mm_mods *mods, int16_t *values, int stride, int bound);

/* LEDs the mode holds: up to cap (key code, value) pairs; 0 when the mode is off (W1). */
#define MM_MAX_LEDS (1 + (MM_K_FX - MM_K_TRIG + 1) + MM_TRACKS)
int mm_leds(const mm_t *s, uint8_t *code, uint8_t *value, int cap);

/* The stored configuration (W3): a versioned blob, MM_BLOB_SIZE bytes. */
#define MM_BLOB_VERSION 2
#define MM_BLOB_SIZE (5 + MM_MACROS * MM_SLOTS * 5 + MM_MACROS * MM_NAME_LEN + 2)
#define MM_BLOB_V1_SIZE (5 + MM_MACROS * MM_SLOTS * 4 + 2) /* version 1: depth -64 .. +63 in a byte, no names */
int mm_encode(const mm_t *s, uint8_t *out);         /* -> MM_BLOB_SIZE */
int mm_decode(mm_t *s, const uint8_t *in, int len); /* 0, or -1 (unknown version, bad size or check) */
/* A setup kept in the project (docs/findings/s7-project-storage.md): "MS", flags, the slots and names as in the
 * blob, a CRC-16; MM_SETUP_SIZE bytes. Unpacking keeps the knobs' positions. */
#define MM_SETUP_SIZE (3 + MM_MACROS * MM_SLOTS * 5 + MM_MACROS * MM_NAME_LEN + 2)
#define MM_SETUP_PROJECT 0x01 /* written while the setups were per project */
void mm_setup_pack(const mm_t *s, int flags, uint8_t *out);
int mm_setup_valid(const uint8_t *in);
int mm_setup_unpack(mm_t *s, const uint8_t *in); /* its flags, or -1 */

/* Per project or per pattern (PERSONALIZE > MACRO SETUP): the setups the project's records carry, one per pattern
 * and the project's. The firmware's save and load call mm_scope_save and mm_scope_load per pattern; the tick calls
 * mm_scope_edited after a change and mm_scope_follow with the pattern on. */
#define MM_PATTERNS 128
enum { MM_SCOPE_PROJECT = 0, MM_SCOPE_PATTERN };
typedef struct {
    uint8_t pat[MM_PATTERNS][MM_SETUP_SIZE]; /* all zero: none yet */
    uint8_t proj[MM_SETUP_SIZE];
    uint8_t mode;              /* MM_SCOPE_* */
    volatile uint8_t loads;    /* bumped by each record loaded */
    uint8_t seen;              /* loads, as mm_scope_follow last saw it */
    volatile uint8_t proj_loaded; /* a record written per project was loaded */
    int16_t cur;               /* the pattern whose setup is on, -1 none */
} mm_scope;
void mm_scope_save(const mm_scope *p, int pattern, uint8_t *out);  /* MM_SETUP_SIZE bytes; pattern -1: not one */
void mm_scope_load(mm_scope *p, int pattern, const uint8_t *in);
void mm_scope_edited(mm_scope *p, const mm_t *s);
int mm_scope_follow(mm_scope *p, mm_t *s, int pattern);            /* 1 when the setup changed */
void mm_scope_set(mm_scope *p, mm_t *s, int mode);

/* A macro's name from the naming screen: up to MM_NAME_LEN characters, trailing spaces dropped. */
void mm_set_name(mm_t *s, int macro, const char *name);

/* A card sector holding the blob (W3): "MMCF", a sequence number, the blob; two copies, the newer valid one wins. */
#define MM_RECORD_SIZE 512
#define MM_RECORD_SCOPE (8 + MM_BLOB_SIZE) /* 0x80 | MM_SCOPE_*; 0 in older records: per project */
void mm_record_encode(const mm_t *s, uint32_t seq, uint8_t *out); /* MM_RECORD_SIZE bytes */
/* Which copy to load: 0 or 1, -1 when neither holds a valid record; *seq gets its sequence number. */
int mm_record_pick(const mm_t *s, const uint8_t *a, const uint8_t *b, uint32_t *seq);

/* Destinations. */
int mm_dest_pos(const mm_dests *d, int dest);       /* its place in the list, -1 for none or unknown */
/* The destination list's lines: none, then per page a separator and its destinations. */
int mm_dest_lines(const mm_dests *d);
int mm_dest_line(const mm_dests *d, int dest);      /* the line of a destination (0 for none) */
int mm_line_dest(const mm_dests *d, int line, int *group_first); /* the destination on a line, -1 on a separator */
void mm_dest_label(const mm_dests *d, int dest, char *buf); /* 8 bytes */
const char *mm_dest_group(const mm_dests *d, int dest);     /* the column heading: SRC, FLTR, ...; META for none */

/* Screens: fill(ctx, x0, y0, x1, y1, colour) with y = 0 the top row, corners inclusive; colour 0 clears, 1 sets,
 * -1 inverts. text(ctx, x, ytop, flags, s) writes s with its top row at ytop, from x or centred on it
 * (MM_TEXT_CENTRE), in the firmware's 5-pixel font or, with MM_TEXT_SMALL, its narrow one (3-pixel letters: the
 * stock page box, the lists); without it the mode uses its own 3 x 5 font. The macro screen leaves rows
 * 0 .. MM_HEADER - 1 (the stock header: pattern, name, tempo) to the firmware. */
#define MM_HEADER 10
#define MM_TEXT_CENTRE 1
#define MM_TEXT_SMALL 2
typedef void mm_fill_fn(void *ctx, int x0, int y0, int x1, int y1, int colour);
typedef void mm_text_fn(void *ctx, int x, int ytop, int flags, const char *s);
typedef struct {
    mm_fill_fn *fill;
    void *ctx;
    mm_text_fn *text;
} mm_canvas;
void mm_draw(const mm_t *s, const mm_canvas *c);

#endif
