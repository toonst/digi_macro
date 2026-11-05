/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: states and the input map (PLAN.md 4.1 and 4.2). */
#include "mm.h"

static int is_taken(const mm_t *s, int code)
{
    return code >= 0 && code < 64 && (s->taken[code >> 5] >> (code & 31)) & 1;
}

static void set_taken(mm_t *s, int code, int on)
{
    uint32_t bit;
    if (code < 0 || code >= 64)
        return;
    bit = (uint32_t)1 << (code & 31);
    if (on)
        s->taken[code >> 5] |= bit;
    else
        s->taken[code >> 5] &= ~bit;
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* FUNC + NO (the stock reload): every macro back to the centre; the pattern is not reloaded. */
static void centre_all(mm_t *s)
{
    for (int i = 0; i < MM_MACROS; i++)
        if (s->m[i].pos) {
            s->m[i].pos = 0;
            s->dirty_mods = 1;
        }
    s->popup = 0;
    s->dirty_screen = 1;
}

/* LEVEL/DATA on the macro screen: every macro moves by delta. */
static void offset_all(mm_t *s, int delta)
{
    for (int i = 0; i < MM_MACROS; i++) {
        int v = clamp(s->m[i].pos + delta, MM_POS_MIN, MM_POS_MAX);
        if (v != s->m[i].pos) {
            s->m[i].pos = (int8_t)v;
            s->dirty_mods = 1;
        }
    }
    s->dirty_screen = 1;
}

static int is_page(int code)
{
    return code >= MM_K_TRIG && code <= MM_K_FX;
}

void mm_init(mm_t *s, const mm_dests *dests)
{
    uint8_t *p = (uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++)
        p[i] = 0;
    s->dests = dests;
    for (int k = 0; k < MM_MACROS; k++)
        for (int j = 0; j < MM_SLOTS; j++)
            s->m[k].slot[j].tracks = (uint16_t)((1u << MM_TRACKS) - 1); /* every track until the setup says less */
}

/* The destination list of the setup screen: shown while a destination knob turns, closed by NO (as the stock). */
static void close_list(mm_t *s)
{
    if (s->list) {
        s->list = 0;
        s->dirty_screen = 1;
    }
}

static void enter(mm_t *s)
{
    s->state = MM_MACRO;
    s->announce = 1;
    s->pushes = 0;
    s->popup = 0;
    s->mod_armed = 0;
    s->page_armed = 0;
    s->dirty_screen = s->dirty_leds = 1;
}

static void leave(mm_t *s)
{
    s->name_arm = 0;
    if (s->state == MM_SETUP)
        s->dirty_config = 1;        /* leaving from the setup screen stores it too */
    s->list = 0;
    s->state = MM_OFF;
    s->pushes = 0;
    s->popup = 0;
    s->dirty_screen = s->dirty_leds = 1;
}

static void open_setup(mm_t *s, int macro)
{
    s->state = MM_SETUP;
    s->edit = (uint8_t)macro;
    s->popup = 0;
    s->dirty_screen = s->dirty_leds = 1;
}

static void close_setup(mm_t *s)
{
    s->list = 0;
    s->state = MM_MACRO;
    s->dirty_screen = s->dirty_leds = 1;
    s->dirty_config = 1;
}

static int is_trig(int code)
{
    return code >= MM_K_TRIG1 && code < MM_K_TRIG1 + MM_TRACKS;
}

static int is_push(int code)
{
    return code >= MM_K_PUSH_A && code < MM_K_PUSH_A + MM_MACROS;
}

/* A FUNC + push let go before the hold: the setup of that macro (from the macro screen) or a switch to it. */
static void func_push_short(mm_t *s)
{
    int i = s->name_arm - 1;
    s->name_arm = 0;
    if (s->state == MM_MACRO) {
        open_setup(s, i);
    } else if (s->state == MM_SETUP) {
        s->edit = (uint8_t)i;
        s->list = 0;
        s->dirty_config = s->dirty_screen = s->dirty_leds = 1;
    }
}

static int trk_held(const mm_t *s)
{
    return s->key_down ? s->key_down(MM_K_TRK) : s->trk;
}

/* The input map (PLAN.md 4.2). Macro mode is a page: each screen takes only the keys it gives a meaning of its own,
 * and every other key keeps its stock function, as on any page (D6): PLAY, STOP, RECORD, PAGE, PTN, SONG, TRK,
 * FUNC, the trig keys, PRESET, SETTINGS, SAMPLES, TEMPO, KEYBOARD, the arrows, YES, the Digitone II's own keys.
 * The menus and popups they open come over the mode as over a page, and the mode is back when they close. */

/* A press on the macro screen: 1 takes it. */
static int press_macro(mm_t *s, int code)
{
    if (code == MM_K_MOD) {         /* also with PAGE held: PAGE + MOD exits (4.1) */
        leave(s);
        return 1;
    }
    if (is_push(code)) {
        int i = code - MM_K_PUSH_A;
        if (s->func) {              /* only FUNC + push: short, the setup; held, the macro's name (4.2) */
            s->name_arm = (uint8_t)(i + 1);
            s->name_ticks = 0;
        }
        s->pushes |= (uint8_t)(1u << i);
        return 1;
    }
    if ((code == MM_K_NO && s->func) || code == MM_K_LEVEL) { /* FUNC + NO (reload), LEVEL/DATA pushed */
        centre_all(s);
        return 1;
    }
    if (code == MM_K_NO && s->pushes) { /* push + NO: back to the centre (D3) */
        for (int i = 0; i < MM_MACROS; i++)
            if (s->pushes & (1u << i) && s->m[i].pos) {
                s->m[i].pos = 0;
                s->dirty_mods = 1;
            }
        s->dirty_screen = 1;
        return 1;
    }
    return 0;
}

/* A press on the setup screen: 1 takes it. */
static int press_setup(mm_t *s, int code)
{
    mm_macro *m = &s->m[s->edit];
    if (code == MM_K_MOD) {         /* back to the macro screen, PAGE held or not (D8) */
        close_setup(s);
        return 1;
    }
    if (s->list && (code == MM_K_NO || code == MM_K_YES)) {
        close_list(s);              /* the choice was applied while turning */
        return 1;
    }
    if (code == MM_K_LEVEL) {       /* LEVEL/DATA pushed: the edited macro to the centre */
        if (m->pos) {
            m->pos = 0;
            s->dirty_mods = 1;
        }
        s->dirty_screen = 1;
        return 1;
    }
    if (is_push(code))
        close_list(s);
    if (is_push(code)) {
        int i = code - MM_K_PUSH_A;
        if (s->func) {              /* short, that macro's setup; held, its name */
            s->name_arm = (uint8_t)(i + 1);
            s->name_ticks = 0;
        } else {
            s->sel = (uint8_t)(i & 3);
        }
        s->pushes |= (uint8_t)(1u << i);
        s->dirty_screen = s->dirty_leds = 1;
        return 1;
    }
    if (code == MM_K_NO && s->func) {
        centre_all(s);
        return 1;
    }
    if (code == MM_K_NO) {
        if (!s->pushes) {
            close_setup(s);
            return 1;
        }
        for (int i = 0; i < MM_MACROS; i++) { /* push + NO resets that knob */
            if (!(s->pushes & (1u << i)))
                continue;
            if (i < MM_SLOTS)
                m->slot[i].dest = 0;
            else
                m->slot[i - MM_SLOTS].depth = 0;
        }
        s->dirty_mods = s->dirty_screen = s->dirty_config = 1;
        return 1;
    }
    if (code == MM_K_LEFT || code == MM_K_RIGHT) {
        close_list(s);
        s->sel = (uint8_t)clamp(s->sel + (code == MM_K_RIGHT ? 1 : -1), 0, MM_SLOTS - 1);
        s->dirty_screen = s->dirty_leds = 1;
        return 1;
    }
    if (is_trig(code) && !trk_held(s)) { /* TRK + trig selects that track, as on a page */
        m->slot[s->sel].tracks ^= (uint16_t)(1u << (code - MM_K_TRIG1));
        s->dirty_mods = s->dirty_screen = s->dirty_config = s->dirty_leds = 1;
        return 1;
    }
    return 0;
}

int mm_key(mm_t *s, int code, int flags)
{
    /* Bit 0 is "down": a release has it clear (FUNC's and PAGE's come with no other bit set, others with 0x10), a held
     * key repeats with it set and 0x08 (digikit's emulator, tests/emu: the mode's key log). */
    if (!(flags & MM_F_PRESS) || flags & MM_F_REPEAT) {
        int took = is_taken(s, code);
        if (!(flags & MM_F_PRESS)) {
            set_taken(s, code, 0);
            if (code == MM_K_FUNC)
                s->func = 0;
            else if (code == MM_K_MOD)
                s->mod_armed = 0;
            else if (code == MM_K_PAGE)
                s->page_armed = 0;
            else if (code == MM_K_TRK)
                s->trk = 0;
            else if (is_push(code)) {
                s->pushes &= (uint8_t)~(1u << (code - MM_K_PUSH_A));
                if (s->name_arm == code - MM_K_PUSH_A + 1)
                    func_push_short(s);
            }
        }
        return took;                /* a release follows its press (4.1) */
    }
    int take;
    if (s->key_down) {              /* what is held now, from the panel: a release the firmware kept is no matter */
        s->func = (uint8_t)(code == MM_K_FUNC || flags & MM_F_FUNC || s->key_down(MM_K_FUNC));
        for (int i = 0; i < MM_MACROS; i++)
            if (MM_K_PUSH_A + i != code && !s->key_down(MM_K_PUSH_A + i))
                s->pushes &= (uint8_t)~(1u << i);
        if (code != MM_K_MOD && !s->key_down(MM_K_MOD))
            s->mod_armed = 0;
        if (code != MM_K_PAGE && !s->key_down(MM_K_PAGE))
            s->page_armed = 0;
    } else if (code == MM_K_FUNC || flags & MM_F_FUNC) { /* the firmware's own "FUNC held" bit counts too */
        s->func = 1;
    }
    if (code == MM_K_TRK)
        s->trk = 1;
    if (s->state != MM_OFF && (is_page(code) || (code == MM_K_MOD && s->func))) {
        leave(s);                   /* a page key leaves macro mode, as any page, and opens its page (FUNC + MOD: the */
        take = 0;                   /* one it opens on any page); over a firmware view too, which the firmware closes */
    } else if (s->state != MM_OFF && s->covered && s->covered()) {
        take = 0;                   /* a firmware view is over the mode: every other key is that view's, MOD too */
                                    /* (it closes a menu, and the mode is back, as a page key brings its page back) */
    } else if (s->state == MM_OFF) {
        take = 0;
        if (code == MM_K_MOD || code == MM_K_PAGE) {
            int other = code == MM_K_MOD ? s->page_armed : s->mod_armed;
            if (other && (!s->can_enter || s->can_enter())) {
                enter(s);           /* the second key of PAGE + MOD, either order (D15) */
                take = 1;
            } else if (code == MM_K_MOD) {
                s->mod_armed = 1;
            } else {
                s->page_armed = 1;
            }
        }
    } else if (s->state == MM_MACRO) {
        take = press_macro(s, code);
    } else {
        take = press_setup(s, code);
    }
    set_taken(s, code, take);
    return take;
}

static int func_held(const mm_t *s)
{
    return s->key_down ? s->key_down(MM_K_FUNC) : s->func;
}

/* 1 when one of the macro's slots is on SRC > TUNE. */
static int has_tune(const mm_t *s, const mm_macro *m)
{
    for (int j = 0; j < MM_SLOTS; j++) {
        int p = mm_dest_pos(s->dests, m->slot[j].dest);
        const char *n = p >= 0 ? s->dests->d[p].name : 0;
        if (n && s->dests->d[p].group == MM_G_SRC && n[0] == 'T' && n[1] == 'U' && n[2] == 'N' && n[3] == 'E' && !n[4])
            return 1;
    }
    return 0;
}

/* FUNC + turn of a macro on TUNE: the next fifth or octave, +7, +12, +19, +24 ... and -5, -12, -17, -24 ...: the
 * positions that are 0 or 7 modulo 12. Past the last one in range the knob stays. */
static int fifth_step(int pos, int up)
{
    for (int v = pos + (up ? 1 : -1); v >= MM_POS_MIN && v <= MM_POS_MAX; v += up ? 1 : -1) {
        int r = ((v % 12) + 12) % 12;
        if (r == 0 || r == 7)
            return v;
    }
    return pos;
}

static int step(const mm_t *s, int i, int delta)
{
    return (s->pushes & (1u << i)) && !s->enc_scaled ? delta * MM_COARSE : delta;
}

int mm_enc_takes(const mm_t *s, int enc)
{
    return s->state != MM_OFF && enc >= 1 && enc <= MM_ENC_LEVEL;
}

int mm_enc(mm_t *s, int enc, int delta)
{
    int i = enc - 1;
    if (!delta || !mm_enc_takes(s, enc))
        return 0;
    if (enc == MM_ENC_LEVEL) {      /* D16: every macro on the macro screen, the edited one on SETUP */
        if (s->state == MM_MACRO) {
            offset_all(s, delta);
        } else {
            mm_macro *m = &s->m[s->edit];
            int v = clamp(m->pos + delta, MM_POS_MIN, MM_POS_MAX);
            if (v != m->pos) {
                m->pos = (int8_t)v;
                s->dirty_mods = 1;
            }
            s->dirty_screen = 1;
        }
        return 1;
    }
    if (i < 0 || i >= MM_MACROS)
        return 0;                   /* anything unknown keeps its stock function */
    delta = step(s, i, delta);
    if (s->state == MM_MACRO) {
        mm_macro *m = &s->m[i];
        int v = func_held(s) && has_tune(s, m) ? fifth_step(m->pos, delta > 0) /* one fifth or octave a turn */
                                               : clamp(m->pos + delta, MM_POS_MIN, MM_POS_MAX);
        if (v != m->pos) {
            m->pos = (int8_t)v;
            s->dirty_mods = 1;
        }
        s->popup = (uint8_t)(i + 1);
        s->popup_ticks = MM_POPUP_TICKS;
        s->dirty_screen = 1;
        return 1;
    }
    mm_macro *m = &s->m[s->edit];
    if (i < MM_SLOTS) {             /* A..D: destination of slot 1..4 (D7), in the list */
        mm_slot *sl = &m->slot[i];
        int n = s->dests ? s->dests->count : 0;
        int p = clamp(mm_dest_pos(s->dests, sl->dest) + delta, -1, n - 1);
        int line;
        sl->dest = p < 0 ? 0 : s->dests->d[p].index;
        s->sel = (uint8_t)i;
        if (s->list != i + 1)
            s->list_top = 0;
        s->list = (uint8_t)(i + 1);
        line = mm_dest_line(s->dests, sl->dest);
        if (line < s->list_top + 1)     /* keep the line and the separator above it in view */
            s->list_top = (uint8_t)(line > 0 ? line - 1 : 0);
        if (line > s->list_top + MM_LIST_ROWS - 1)
            s->list_top = (uint8_t)(line - (MM_LIST_ROWS - 1));
    } else {                        /* E..H: depth of slot 1..4 */
        close_list(s);
        mm_slot *sl = &m->slot[i - MM_SLOTS];
        /* scaled: the firmware's filter gives 1/256 units, as for the stock depths; else whole units */
        sl->depth = (int16_t)clamp(sl->depth + (s->enc_scaled ? delta : delta * MM_DEPTH_ONE), MM_DEPTH_MIN,
                                   MM_DEPTH_MAX);
        s->sel = (uint8_t)(i - MM_SLOTS);
    }
    s->dirty_mods = s->dirty_screen = s->dirty_config = s->dirty_leds = 1;
    return 1;
}

int mm_tick(mm_t *s)
{
    if (s->name_arm) {              /* FUNC + push: held long enough names the macro */
        if (s->key_down && !s->key_down(MM_K_PUSH_A + s->name_arm - 1)) {
            func_push_short(s);     /* let go, though no release came */
        } else if (++s->name_ticks >= MM_HOLD_TICKS) {
            s->name_req = s->name_arm;
            s->name_arm = 0;
        }
    }
    if (s->popup && s->popup_ticks && !--s->popup_ticks) {
        s->popup = 0;
        s->dirty_screen = 1;
    }
    if (!s->dirty_screen)
        return 0;
    s->dirty_screen = 0;
    return 1;
}
