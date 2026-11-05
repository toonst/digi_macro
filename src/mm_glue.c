/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode on elekloader's hook bus (core-dt2 1.1, core-dn2 0.3; PLAN.md 2.1 and 5): ev_key, ev_enc, ev_tick,
 * ev_draw, ev_frame_params and ev_key_leds. It draws with the firmware's Bitmap::fillRect (FILLRECT_AT) and, where
 * the device has it, its text routine (TEXT_AT, with the font object at TEXT_FONT).
 *
 * The device's addresses are in mm_os.h, chosen by mod.json's MM_DT2 or MM_DN2; docs/findings has what each is. */
#include "mm_kits.h"
#include "mm_os.h"

#ifndef FILLRECT_AT
#error "FILLRECT_AT: Bitmap::fillRect's address in the target OS (mm_os.h)"
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
    mm_kits_card_restore((pick ? other : mm_sector)[MM_RECORD_SCOPE]);
    mm_seq = seq;
    mm_copy = (uint8_t)pick;
}

static void card_store(mm_t *s)
{
    mm_record_encode(s, mm_seq + 1, mm_sector);
    mm_sector[MM_RECORD_SCOPE] = mm_kits_card();
    if (((block_fn)CARD_WRITE_AT)(CARD_SECTOR + (mm_copy ^ 1), MM_RECORD_SIZE, mm_sector) == 0) {
        mm_seq++;
        mm_copy ^= 1;               /* the next write goes to the other copy */
    }
}
#endif

#ifdef KEY_DOWN_AT
/* The firmware's key-down query (digikit finding 58): 1 while key code c is down, from the panel's own mask. */
static int fw_key_down(int code)
{
    return ((int (*)(int))KEY_DOWN_AT)(code) & 1;
}
#endif

#ifdef ENC_FILTER_AT
/* The firmware's encoder filter (docs/findings/s1-input.md): every stock knob turns through it, so the mode's knobs
 * step as the stock ones do (a lead-in after the knob rests, more per count as it turns faster, coarser while
 * pushed). The macro, depth and LEVEL knobs go the way the stock 0..127 parameters do (filter(block, knob, count, x2
 * pushed, ENC_CFG_PARAM): raw units, 256 a step); the destination knobs as the stock modulation setup's
 * (ENC_LIST_AT(block, event, ENC_CFG_LIST): steps, x8 pushed). The state is a block of the firmware's own class,
 * built once with its constructor, which also starts the timer that refills it. */
typedef int (*enc_filter_t)(void *block, int knob, int count, const void *cfg, int pushed);
typedef int (*enc_list_t)(void *block, void *ev, const void *cfg);
static uint8_t mm_enc_block[ENC_BLOCK_SIZE] __attribute__((aligned(4)));
static uint8_t mm_enc_ready;

static void enc_block(void)
{
    if (!mm_enc_ready) {
        ((void (*)(void *))ENC_BLOCK_AT)(mm_enc_block);
        mm_enc_ready = 1;
    }
}
#endif

/* Firmware views over the mode (docs/findings/s1-input.md, "Views over the mode"). Macro mode is a page: the views the
 * firmware puts over a page (a menu, a dialog, the pattern, bank and song popups, mute mode, the track selection
 * while TRK is held, the naming screen) come over the macro screen the same way. Such a view gets the keys and
 * LEVEL/DATA (mm_t.covered), and it is drawn over the macro screen, which stays under it, with knobs A-H still
 * moving the macros, as they move a page's parameters under it; one that hides the page (an opaque view: a
 * full-screen menu) hides the macro screen as well and gets every knob. The mode stays on and is back when the view
 * closes; a page key leaves it, as it leaves any page.
 *
 * The view controller (Brain + 0x40) keeps its views in a list, bottom first: the sentinel at + 0x14, a node's next
 * at + 0, prev at + 4, its View at + 8 (the vtable at the View's first word, the opaque flag at its + 0x16; views below
 * the topmost opaque one are not drawn), paint at slot 4 of the View vtable. The page's views end with the view
 * that tops the stack on the main screen and the parameter pages (VIEW_TOP_BASE); every view above it is over the
 * page. Two of them leave the mode its own keys:
 * - FUNC's (QuickMuteMenuView, while FUNC is held): FUNC + push, FUNC + NO. The mode draws over its screen.
 * - mute mode's (FUNC + TRK, PermanentMuteMenuView): the trig keys mute and FUNC + trig prepares mutes, as on any
 *   page, but pushes, FUNC + push, MOD and PAGE + MOD are the mode's (D6). It is drawn over the macro screen. It is
 *   known by its class's name (RTTI: the vtable's word before slot 0 is the typeinfo, whose + 4 is the name), the
 *   same on both devices.
 * The knobs go as the firmware sends them (its encoder dispatch, DT2 0x4011c674): down the stack from the top, each
 * view's EncoderHandler (View + 4) is asked, vtable slot 2, handler(view + 4, event) -> bool, and the first that
 * takes the turn ends it. Views that keep the default (0: mute mode, the pattern and bank popups, QuickMute,
 * TrackSelection) pass it on to the page; TEMPO's, the naming screen's and the song screens' take theirs. The glue
 * asks the views over the page the same way, and a turn none of them takes is the mode's, as it would be the
 * page's. */
#ifdef VIEW_TOP_BASE
#define NODE_NEXT(n) (*(char **)(n))
#define NODE_PREV(n) (*(char **)((n) + 4))
#define NODE_VIEW(n) (*(char **)((n) + 8))
#define VIEW_VT(v) (*(uint32_t **)(v))
#define VIEW_OPAQUE(v) (*(uint8_t *)((v) + 0x16))
#define VIEW_PAINT 4
#define STACK_MAX 64                /* a bound on the walk: the stack holds about a dozen views */
#define VIEW_ENC 4                  /* the View's EncoderHandler; its handler is slot 2 of that vtable */
#define VIEW_CLOSING 0x30           /* the View closed itself: the controller sweeps it (+ 0x22) */
#define DDR_LO 0x40000000u          /* where a vtable, a typeinfo and a name may be: the DDR the OS runs in */
#define DDR_HI 0x48000000u
#define MUTE_CLASS "21PermanentMuteMenuView"
/* OVER: a view to draw over the mode; OVER_OPAQUE: one hides the page; OVER_KEYS: one takes the keys */
enum { OVER = 1, OVER_OPAQUE = 2, OVER_KEYS = 4 };

uint32_t mm_mute_vt;                /* mute mode's view's vtable, once seen (the emulator scenarios read it) */

static int in_ddr(uint32_t a, uint32_t len)
{
    return a >= DDR_LO && a < DDR_HI - len && !(a & 1);
}

/* 1 when the class of the view whose vtable is vt is named (mangled) name: RTTI, vtable[-1] the typeinfo, its + 4
 * the name. */
static int is_class(uint32_t vt, const char *name)
{
    uint32_t ti, nm;
    const char *p;
    if (!in_ddr(vt - 4, 8))
        return 0;
    ti = ((const uint32_t *)vt)[-1];
    if (!in_ddr(ti, 8))
        return 0;
    nm = ((const uint32_t *)ti)[1];
    if (nm < DDR_LO || nm >= DDR_HI - 64)
        return 0;
    for (p = (const char *)nm; *name; p++, name++)
        if (*p != *name)
            return 0;
    return *p == 0;
}

static int is_mute(uint32_t vt)
{
    if (!mm_mute_vt && is_class(vt, MUTE_CLASS))
        mm_mute_vt = vt;
    return vt == mm_mute_vt;
}

static char *stack_sentinel(void)
{
    return (char *)mm_brain + 0x40 + 0x14;
}

/* What is over the page: 0, OVER (views the mode is drawn under), or OVER | OVER_OPAQUE (one of them hides the
 * page). *base gets the page's top node, 0 when there is none (the firmware then keeps the keys and the screen). */
static int over_page(char **base)
{
    char *sentinel, *n;
    int over = 0, k = 0;
    *base = 0;
    if (!mm_brain)
        return 0;
    sentinel = stack_sentinel();
    for (n = NODE_PREV(sentinel); n != sentinel && k < STACK_MAX; n = NODE_PREV(n), k++) {
        char *v = NODE_VIEW(n);
        uint32_t vt = (uint32_t)VIEW_VT(v);
        if (vt == VIEW_TOP_BASE) {
            *base = n;
            return over;
        }
        if (vt == VIEW_FUNC)
            continue;
        over |= OVER | (VIEW_OPAQUE(v) ? OVER_OPAQUE : 0) | (is_mute(vt) ? 0 : OVER_KEYS);
    }
    return OVER | OVER_OPAQUE | OVER_KEYS;
}

static int covered(void)            /* 1: the keys go to the view over the mode */
{
    char *base;
    return mm_state.state != MM_OFF && (over_page(&base) & (OVER_KEYS | OVER_OPAQUE)) != 0;
}

/* A turn, offered to the views over the page as the firmware's dispatch offers it: 1 when one took it. */
static int over_takes_enc(void *ev)
{
    char *sentinel, *n;
    int k = 0;
    if (!mm_brain)
        return 0;
    sentinel = stack_sentinel();
    for (n = NODE_PREV(sentinel); n != sentinel && k < STACK_MAX; n = NODE_PREV(n), k++) {
        char *v = NODE_VIEW(n), *eh = v + VIEW_ENC;
        if ((uint32_t)VIEW_VT(v) == VIEW_TOP_BASE)
            return 0;               /* the page: the mode's */
        if (((uint8_t (*)(void *, void *))(*(uint32_t **)eh)[2])(eh, ev)) {
            if (v[VIEW_CLOSING])
                *((uint8_t *)mm_brain + 0x40 + 0x22) = 1;
            return 1;
        }
    }
    return 0;
}

/* D9: macro mode starts only on the main screen and the parameter pages, where nothing but FUNC's view and mute
 * mode's is over the page: no menu, dialog or popup. */
static int fw_can_enter(void)
{
    char *base;
    int over = over_page(&base);
    return base && !(over & (OVER_OPAQUE | OVER_KEYS));
}

static void paint(char *node, void *bmp)
{
    char *v = NODE_VIEW(node);
    ((void (*)(void *, void *))VIEW_VT(v)[VIEW_PAINT])(v, bmp);
}

static void draw_mode(mm_t *s, void *bmp);

/* The frame as the firmware composes it, with the mode as the page's top view: the page's views (from the topmost
 * opaque one, as the firmware draws them), the macro screen, then the views over the page, in their order. */
static void compose(mm_t *s, void *bmp, char *base)
{
    char *sentinel = stack_sentinel(), *n = base;
    int k;
    for (k = 0; n != sentinel && !VIEW_OPAQUE(NODE_VIEW(n)) && k < STACK_MAX; k++)
        n = NODE_PREV(n);
    if (n == sentinel || k == STACK_MAX)
        n = NODE_NEXT(sentinel);    /* none is opaque: from the bottom */
    FILLRECT(bmp, 0, 0, 127, 63, 0);
    for (k = 0; n != sentinel && k < STACK_MAX; n = NODE_NEXT(n), k++) {
        paint(n, bmp);
        if (n == base)
            break;
    }
    draw_mode(s, bmp);
    for (n = NODE_NEXT(base), k = 0; n != sentinel && k < STACK_MAX; n = NODE_NEXT(n), k++)
        if ((uint32_t)VIEW_VT(NODE_VIEW(n)) != VIEW_FUNC)
            paint(n, bmp);
}
#else
static int covered(void)
{
    return 0;
}
#endif

/* The firmware returns pointers in d0; this compiler's ABI would read a pointer result from a0. So routines that
 * return a pointer are called as returning a number. */
typedef uint32_t (*fw_ptr_t)(void);
typedef uint32_t (*fw_new_t)(unsigned bytes);

#ifdef HEADER_MSG_AT
/* The header's message, as a page shows its title there when it opens ("Filter (1/2)"): (header, fmt, ...), the
 * header from HEADER_OBJ_AT() (docs/findings/s4-screens.md). On the tick, out of the key handler: the header's own
 * time base. */
void mm_notice(const char *msg)
{
    void *ui = (void *)((fw_ptr_t)HEADER_OBJ_AT)();
    ((void (*)(void *, const char *, ...))HEADER_MSG_AT)(ui, "%s", msg);
}
#else
void mm_notice(const char *msg)
{
    (void)msg;
}
#endif

static void announce(void)
{
    mm_notice("Macro Mode");
}

#ifdef NAME_VIEW_AT
/* The firmware's naming screen (NameView), as a MIDI machine opens it to name a parameter (FUNC + hold a knob;
 * docs/findings/s4-screens.md): made like std::make_shared does (one block: the control block, then the view),
 * given the title, the current name, the length and a std::function<void(std::string)> called with the new name on
 * YES, and pushed on the view controller (Brain + 0x40). The function's functor (the macro) is stored in place, so its
 * manager only copies it. */
typedef struct {
    uint32_t functor[2];
    int (*manager)(void *dest, const void *src, int op);
    void (*invoker)(const void *functor, char **name);
} fw_function;

static int name_manager(void *dest, const void *src, int op)
{
    if (op == 0)                    /* the type's info: none */
        *(void **)dest = 0;
    else if (op == 1)               /* the functor: in place */
        *(const void **)dest = src;
    else if (op == 2) {             /* a copy */
        ((uint32_t *)dest)[0] = ((const uint32_t *)src)[0];
        ((uint32_t *)dest)[1] = ((const uint32_t *)src)[1];
    }
    return 0;                       /* 3, destroy: nothing to free */
}

/* std::string is one pointer to its characters, NUL-terminated; the caller keeps it */
static void name_invoke(const void *functor, char **name)
{
    mm_set_name(&mm_state, (int)((const uint32_t *)functor)[0], *name);
}

static void open_name_view(int macro)
{
    typedef void (*str_t)(void *str, const char *cstr, void *alloc);
    typedef void (*name_view_t)(void *view, void *title, void *name, int a, int len, int b, fw_function *done);
    uint32_t *block = (uint32_t *)((fw_new_t)OP_NEW_AT)(0xb0), sp[2];
    char *title, *name, alloc;
    fw_function done = {{(uint32_t)macro, 0}, name_manager, name_invoke};
    if (!block)
        return;
    block[0] = SP_INPLACE_VT;       /* the control block: one use, one weak, the view after it */
    block[1] = 1;
    block[2] = 1;
    block[3] = (uint32_t)(block + 4);
    ((str_t)STR_NEW_AT)(&title, "MACRO NAME", &alloc);
    ((str_t)STR_NEW_AT)(&name, mm_state.m[macro].name, &alloc);
    ((name_view_t)NAME_VIEW_AT)(block + 4, &title, &name, 0, MM_NAME_LEN, 0, &done);
    ((void (*)(void *))STR_FREE_AT)(&title);
    ((void (*)(void *))STR_FREE_AT)(&name);
    sp[0] = (uint32_t)(block + 4);  /* shared_ptr<View> */
    sp[1] = (uint32_t)block;
    ((void (*)(void *, void *, int))VIEW_PUSH_AT)((char *)mm_brain + 0x40, sp, 0);
    ((void (*)(void *))SP_RELEASE_AT)(&sp[1]); /* the view controller holds it now */
}
#endif

static mm_t *state(void)
{
    if (!mm_state.dests) {
        mm_init(&mm_state, mm_device_dests());
#ifdef VIEW_TOP_BASE
        mm_state.can_enter = fw_can_enter;
        mm_state.covered = covered;
#endif
#ifdef KEY_DOWN_AT
        mm_state.key_down = fw_key_down;
#endif
#ifdef ENC_FILTER_AT
        mm_state.enc_scaled = 1;
#endif
    }
    return &mm_state;
}

mm_t *mm_get(void)
{
    return state();
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
static void fw_text(void *bmp, int x, int ytop, int flags, const char *str)
{
    void *font = (void *)(flags & MM_TEXT_SMALL ? TEXT_FONT_SMALL : TEXT_FONT);
    ((text_t)TEXT_AT)(bmp, font, x, 59 - ytop, flags & MM_TEXT_CENTRE ? 2 : 0, "%s", str);
}
#endif

/* The last key events (code << 16 | flags << 8 | taken), for the emulator scenarios. */
uint32_t mm_keylog[16];
uint8_t mm_keylog_n;

/* Every key goes through mm_key, under a firmware view too (mm_t.covered): there it takes nothing, but a page key
 * still leaves the mode and a release still follows its press. */
int mm_ev_key(void *brain, void *ev)
{
    mm_t *s = state();
    int take;
    mm_brain = brain;
    take = mm_key(s, *(int *)((char *)ev + 12), *(int *)((char *)ev + 16));
    mm_keylog[mm_keylog_n++ & 15] = (uint32_t)(*(int *)((char *)ev + 12) << 16 | (*(int *)((char *)ev + 16) & 0xff) << 8 | take);
    return take;
}

int mm_ev_enc(void *brain, void *ev)
{
    mm_t *s = state();
    int enc = *(int *)((char *)ev + 12);
    (void)brain;
    if (!mm_enc_takes(s, enc))
        return 0;                   /* off, or a knob the mode does not know */
#ifdef VIEW_TOP_BASE
    if (over_takes_enc(ev))
        return 1;                   /* a view over the mode took it, as over a page */
#endif
#ifdef ENC_FILTER_AT
    {
        int pushed = *((uint8_t *)ev + 20), steps;
        enc_block();
        if (s->state == MM_SETUP && enc <= MM_SLOTS) {
            steps = ((enc_list_t)ENC_LIST_AT)(mm_enc_block, ev, (const void *)ENC_CFG_LIST);
        } else if (s->state == MM_SETUP && enc <= MM_MACROS) { /* depths: 1/256 units, as the stock depths */
            int count = *(int *)((char *)ev + 16) * (pushed ? 12 : 1);
            steps = ((enc_filter_t)ENC_FILTER_AT)(mm_enc_block, enc - 1, count, (const void *)ENC_CFG_DEPTH, pushed);
        } else {
            int count = *(int *)((char *)ev + 16) * (pushed ? 2 : 1);
            int raw = ((enc_filter_t)ENC_FILTER_AT)(mm_enc_block, enc - 1, count, (const void *)ENC_CFG_PARAM, pushed);
            steps = raw >= 0 ? raw >> 8 : -(-raw >> 8);
        }
        if (steps)
            mm_enc(s, enc, steps);
        return 1;                   /* the mode's knob, moved or not */
    }
#endif
    return mm_enc(s, enc, *(int *)((char *)ev + 16));
}

void mm_ev_tick(void *ctrl)
{
    mm_t *s = state();
#ifdef ENC_FILTER_AT
    enc_block();
#endif
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
#endif
    mm_kits_tick(s);
#ifdef CARD_WRITE_AT
    if (s->dirty_config && s->state != MM_SETUP) {  /* once per visit of the setup screen, not per turn */
        card_store(s);
        s->dirty_config = 0;
    }
#else
    s->dirty_config = 0;
#endif
#ifdef VIEW_TOP_BASE
    {
        static uint8_t was_over;
        char *base;
        uint8_t now = (uint8_t)(s->state != MM_OFF ? over_page(&base) : 0);
        if (now != was_over)        /* a firmware view came or went: the frame and the LEDs again */
            s->dirty_screen = s->dirty_leds = 1;
        was_over = now;
    }
#endif
    if (mm_tick(s))
        *((unsigned char *)ctrl + 0x20) = 1;  /* recompose the frame */
    if (s->announce) {              /* on the tick, out of the key handler: the header's own time base */
        s->announce = 0;
        announce();
    }
    if (s->name_req) {
#ifdef NAME_VIEW_AT
        if (mm_brain && !covered())
            open_name_view(s->name_req - 1);
#endif
        s->name_req = 0;
    }
    if (s->dirty_leds) {
        *((unsigned char *)ctrl + 0x21) = 1;  /* run the key-LED frame (docs/findings/s2-leds.md) */
        s->dirty_leds = 0;
    }
}

static void draw_mode(mm_t *s, void *bmp)
{
#ifdef TEXT_AT
    mm_canvas c = {fw_fill, bmp, fw_text};
#else
    mm_canvas c = {fw_fill, bmp, 0};
#endif
    /* the macro screen keeps the stock header (pattern, name, tempo); the setup screen has its own title */
    FILLRECT(bmp, 0, 0, 127, s->state == MM_MACRO ? 63 - MM_HEADER : 63, 0);
    mm_draw(s, &c);
}

void mm_ev_draw(void *bmp, void *ctrl)
{
    mm_t *s = state();
    (void)ctrl;
    if (s->state == MM_OFF)
        return;
#ifdef VIEW_TOP_BASE
    {
        char *base;
        int over = over_page(&base);
        if (over & OVER_OPAQUE)
            return;                 /* a view that hides the page hides the mode: the firmware's frame */
        if (over) {
            compose(s, bmp, base);  /* the mode under the views over it */
            return;
        }
    }
#endif
    draw_mode(s, bmp);
}

/* W2: ev_frame_params (core-dt2 1.1, core-dn2 0.3): each frame's per-track values, before the stock modulators. Interrupt
 * level: it only reads the lists the UI task published. */
void mm_ev_frame_params(int16_t *values, int stride)
{
    mm_mods *m = mm_live;
    if (m)
        mm_apply(m, values, stride, mm_state.dests->bound);
}

#ifdef LEDSET_AT
/* W1: ev_key_leds (core-dt2 1.1, core-dn2 0.3). LedManager's setter (mgr, logical id, layer, value): layer 2 lights the LED with
 * the value, layer 1 turns it off, and either keeps the views' later sets out (docs/findings/s2-leds.md). The logical
 * id of key code c is c + 19 for the page keys (TRIG 20 .. MOD 25) and c + 1 for the trig keys (digikit finding 19,
 * devices/digit*-ii-leds.toml: the same on both). */
typedef void (*ledset_t)(void *mgr, int logical, int layer, int value);
void mm_ev_key_leds(void *mgr)
{
    uint8_t code[MM_MAX_LEDS], value[MM_MAX_LEDS];
    int n;
#ifdef MM_LED_TEST
    /* A colour test (MM_LED_TEST=1 tools/build.sh): on the macro screen trig key n shows (n - 1) << 4 | 1, and TRIG,
     * SRC, FLTR, AMP, FX show 0x11, 0x21, 0x31, 0x51, 0x61, so one look tells which value is which colour. */
    if (mm_state.state == MM_MACRO) {
        static const uint8_t pages[5] = {0x11, 0x21, 0x31, 0x51, 0x61};
        for (int t = 0; t < MM_TRACKS; t++)
            ((ledset_t)LEDSET_AT)(mgr, MM_K_TRIG1 + t + 1, 2, t << 4 | 1);
        for (int k = 0; k < 5; k++)
            ((ledset_t)LEDSET_AT)(mgr, MM_K_TRIG + k + 19, 2, pages[k]);
    }
#endif
    n = mm_leds(&mm_state, code, value, MM_MAX_LEDS);
    for (int i = 0; i < n; i++)
        ((ledset_t)LEDSET_AT)(mgr, code[i] <= MM_K_MOD ? code[i] + 19 : code[i] + 1, value[i] == MM_LED_OFF ? 1 : 2,
                              value[i]);
}
#endif
