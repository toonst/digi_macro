/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Draw tests: what the screens put where. */
#include <stdio.h>

#include "raster.h"

static int *nchecks, *nfail;
#define CHECK(c)                                                                \
    do {                                                                        \
        ++*nchecks;                                                             \
        if (!(c)) {                                                             \
            ++*nfail;                                                           \
            printf("%s:%d: %s: CHECK(%s)\n", __FILE__, __LINE__, __func__, #c); \
        }                                                                       \
    } while (0)

static const uint8_t idx[] = {25, 30, 55};
static const char *const names[] = {"TUNE", "FLT FRQ", "DEL SND"};
static const mm_dests dests = {idx, names, 3, 70};
static raster r;

static void macro_screen(void)
{
    mm_t s;
    memset(&s, 0, sizeof s);
    s.dests = &dests;
    raster_draw(&r, &s);
    CHECK(r.calls == 0);                        /* off: the stock screen stays */

    s.state = MM_MACRO;
    raster_draw(&r, &s);
    CHECK(r.out_of_bounds == 0);
    for (int k = 0; k < MM_MACROS; k++) {
        int cx = 16 + 32 * (k & 3), cy = 12 + 32 * (k >> 2);
        CHECK(r.px[cy - 9][cx]);                /* the rim */
        CHECK(r.px[cy - 4][cx]);                /* the pointer, straight up at the centre */
        CHECK(r.px[cy - 11][cx]);               /* the centre mark */
        CHECK(raster_count(&r, cx - 2, cy + 14, cx + 2, cy + 18) > 0); /* the letter */
    }
    s.m[0].pos = 63;
    raster_draw(&r, &s);
    CHECK(!r.px[12 - 4][16]);                   /* the pointer has turned right */
    CHECK(raster_count(&r, 17, 8, 23, 18) >= 4);

    s.m[1].pos = -64;
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 32 + 9, 8, 32 + 15, 18) >= 4); /* turned left */

    int before = raster_count(&r, 0, 26, 31, 30);
    s.popup = 1;                                /* a turned knob shows its value */
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 0, 26, 31, 30) != before);

    s.popup = 0;
    s.m[2].slot[1].dest = 30;                   /* a set knob shows its first destination */
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 64 + 2, 26, 64 + 29, 30) > 20);
}

static void setup_screen(void)
{
    mm_t s;
    memset(&s, 0, sizeof s);
    s.dests = &dests;
    s.state = MM_SETUP;
    s.edit = 2;
    s.sel = 1;
    s.m[2].slot[0] = (mm_slot){30, 40, 0x0005};
    s.m[2].slot[1] = (mm_slot){55, -12, 0xffff};
    raster_draw(&r, &s);
    CHECK(r.out_of_bounds == 0);
    CHECK(r.px[7][33] && r.px[53][62]);         /* slot 2's column is inverted */
    CHECK(!r.px[7][1] && !r.px[53][30]);        /* slot 1's is not */
    CHECK(raster_count(&r, 0, 0, 30, 4) > 10);  /* the title */
    CHECK(raster_count(&r, 0, 58, 127, 62) > 40); /* the hint line */
    s.sel = 3;
    raster_draw(&r, &s);
    CHECK(r.px[7][97] && !r.px[7][33]);
}

void test_draw(int *checks_out, int *failures_out)
{
    nchecks = checks_out;
    nfail = failures_out;
    macro_screen();
    setup_screen();
}
