/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: the macro screen and the setup screen (PLAN.md 4.4), drawn with one fill callback. The font and the
 * knob are the mod's own, so the only firmware routine it needs is fillRect. [O] spike S4 may swap in the stock
 * font and knob graphic. */
#include "mm.h"

#define W 128
#define H 64

static const uint16_t sin_q10[91] = {
    0,    18,   36,   54,   71,   89,   107,  125,  143,  160,  178,  195,  213,  230,  248,  265,  282,  299,  316,
    333,  350,  367,  384,  400,  416,  433,  449,  465,  481,  496,  512,  527,  543,  558,  573,  587,  602,  616,
    630,  644,  658,  672,  685,  698,  711,  724,  737,  749,  761,  773,  784,  796,  807,  818,  828,  839,  849,
    859,  868,  878,  887,  896,  904,  912,  920,  928,  935,  943,  949,  956,  962,  968,  974,  979,  984,  989,
    994,  998,  1002, 1005, 1008, 1011, 1014, 1016, 1018, 1020, 1022, 1023, 1023, 1024, 1024,
};

/* sin of a in degrees, -180 .. 180, scaled by 1024 */
static int isin(int a)
{
    int neg = a < 0;
    if (neg)
        a = -a;
    if (a > 90)
        a = 180 - a;
    return neg ? -sin_q10[a] : sin_q10[a];
}

static int icos(int a)
{
    return isin(a >= 90 ? a - 270 : a + 90);
}

static void px(const mm_canvas *c, int x, int y)
{
    if (x >= 0 && x < W && y >= 0 && y < H)
        c->fill(c->ctx, x, y, x, y, 1);
}

static void line(const mm_canvas *c, int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        px(c, x0, y0);
        if (x0 == x1 && y0 == y1)
            return;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void circle(const mm_canvas *c, int cx, int cy, int r)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        px(c, cx + x, cy + y), px(c, cx - x, cy + y), px(c, cx + x, cy - y), px(c, cx - x, cy - y);
        px(c, cx + y, cy + x), px(c, cx - y, cy + x), px(c, cx + y, cy - x), px(c, cx - y, cy - x);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* A knob at position v (-64 .. +63): pointer up is the centre, the travel is +-135 degrees. */
/* n / 1024, rounded to the nearest, halves away from zero */
static int q10(int n)
{
    return n >= 0 ? (n + 512) >> 10 : -((-n + 512) >> 10);
}

static void knob(const mm_canvas *c, int cx, int cy, int r, int v)
{
    int a = v * 135 / 64;
    int len = r - 2;
    circle(c, cx, cy, r);
    line(c, cx, cy, cx + q10(isin(a) * len), cy - q10(icos(a) * len));
    px(c, cx, cy - r - 2);          /* the centre mark */
}

/* 3 x 5 glyphs, top row first, bit 2 the left column */
static const struct {
    char ch;
    uint8_t row[5];
} font[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 3, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}}, {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 2, 2, 2}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}}, {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'C', {3, 4, 4, 4, 3}}, {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}}, {'J', {1, 1, 1, 5, 2}},
    {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}}, {'M', {5, 7, 7, 5, 5}}, {'N', {6, 5, 5, 5, 5}},
    {'O', {2, 5, 5, 5, 2}}, {'P', {6, 5, 6, 4, 4}}, {'Q', {2, 5, 5, 7, 3}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}},
    {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}}, {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
    {'+', {0, 2, 7, 2, 0}}, {'-', {0, 0, 7, 0, 0}}, {':', {0, 2, 0, 2, 0}}, {'.', {0, 0, 0, 0, 2}},
    {'/', {1, 1, 2, 4, 4}},
};

static int text_width(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n ? 4 * n - 1 : 0;
}

static void text(const mm_canvas *c, int x, int y, const char *s)
{
    for (; *s; s++, x += 4) {
        char ch = *s >= 'a' && *s <= 'z' ? (char)(*s - 32) : *s;
        unsigned g;
        for (g = 0; g < sizeof font / sizeof font[0] && font[g].ch != ch; g++)
            ;
        if (g == sizeof font / sizeof font[0])
            continue;               /* space, or a character the font lacks */
        for (int r = 0; r < 5; r++) {
            int bits = font[g].row[r];
            for (int col = 0; col < 3;) {   /* one fill per run of set pixels */
                if (!(bits & (4 >> col))) {
                    col++;
                    continue;
                }
                int end = col;
                while (end + 1 < 3 && bits & (4 >> (end + 1)))
                    end++;
                c->fill(c->ctx, x + col, y + r, x + end, y + r, 1);
                col = end + 1;
            }
        }
    }
}

static void text_centre(const mm_canvas *c, int cx, int y, const char *s)
{
    text(c, cx - text_width(s) / 2, y, s);
}

static void fmt_signed(int v, char *buf)
{
    int i = 0;
    char tmp[4];
    int n = 0;
    if (v > 0)
        buf[i++] = '+';
    else if (v < 0)
        buf[i++] = '-', v = -v;
    do
        tmp[n++] = (char)('0' + v % 10);
    while ((v /= 10) && n < 3);
    while (n)
        buf[i++] = tmp[--n];
    buf[i] = 0;
}

/* What a knob of the macro screen is called: its first slot's destination once set, else its letter. */
static void macro_label(const mm_t *s, int k, char *buf)
{
    for (int j = 0; j < MM_SLOTS; j++)
        if (s->m[k].slot[j].dest) {
            mm_dest_label(s->dests, s->m[k].slot[j].dest, buf);
            return;
        }
    buf[0] = (char)('A' + k);
    buf[1] = 0;
}

static void draw_macro(const mm_t *s, const mm_canvas *c)
{
    char buf[8];
    for (int k = 0; k < MM_MACROS; k++) {
        int cx = 16 + 32 * (k & 3), cy = 12 + 32 * (k >> 2);
        knob(c, cx, cy, 9, s->m[k].pos);
        if (s->popup == k + 1)
            fmt_signed(s->m[k].pos, buf);
        else
            macro_label(s, k, buf);
        text_centre(c, cx, cy + 14, buf);
    }
}

static void draw_setup(const mm_t *s, const mm_canvas *c)
{
    char buf[8];
    const mm_macro *m = &s->m[s->edit];
    char title[] = "MACRO A";
    title[6] = (char)('A' + s->edit);
    text(c, 1, 0, title);
    for (int j = 0; j < MM_SLOTS; j++) {
        const mm_slot *sl = &m->slot[j];
        int x0 = 32 * j, cx = x0 + 16;
        int n = 0;
        mm_dest_label(s->dests, sl->dest, buf);
        text_centre(c, cx, 9, buf);                 /* A..D: the destination */
        knob(c, cx, 28, 9, sl->depth);              /* E..H: the depth */
        fmt_signed(sl->depth, buf);
        text_centre(c, cx, 40, buf);
        for (int t = 0; t < MM_TRACKS; t++)
            n += (sl->tracks >> t) & 1;
        if (!n) {
            buf[0] = '-', buf[1] = '-', buf[2] = 0;
        } else if (n == MM_TRACKS) {
            buf[0] = 'A', buf[1] = 'L', buf[2] = 'L', buf[3] = 0;
        } else {
            fmt_signed(n, buf);
            int i = buf[0] == '+' ? 1 : 0;          /* "3 TRK" */
            int o = 0;
            while (buf[i])
                buf[o++] = buf[i++];
            buf[o++] = ' ', buf[o++] = 'T', buf[o++] = 'R', buf[o++] = 'K', buf[o] = 0;
        }
        text_centre(c, cx, 47, buf);
        if (j == s->sel)                            /* the selected parameter, inverted */
            c->fill(c->ctx, x0, 7, x0 + 31, 53, -1);
    }
    text(c, 1, 58, "A-D DEST  E-H DEPTH  TRIG TRK");
}

void mm_draw(const mm_t *s, const mm_canvas *c)
{
    if (s->state == MM_MACRO)
        draw_macro(s, c);
    else if (s->state == MM_SETUP)
        draw_setup(s, c);
}
