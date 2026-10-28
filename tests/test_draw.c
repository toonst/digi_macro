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

static const mm_dest table[] = {
    {25, MM_G_SRC, 0x400, 0x7c00, "TUNE"}, {30, MM_G_FLTR, 0, 0, "FREQ"}, {55, MM_G_FX, 0, 0, "DEL"}};
static const mm_dests dests = {table, 3, 70};
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
    CHECK(raster_count(&r, 0, 0, 127, MM_HEADER - 1) == 0); /* the stock header is left alone */
    CHECK(r.px[13][7] && r.px[36][7] && r.px[24][0] && r.px[24][14]); /* the MAC box */
    CHECK(!r.px[13][0] && !r.px[36][14]);       /* with rounded corners */
    CHECK(raster_count(&r, 2, 22, 12, 26) > 8); /* MAC */
    CHECK(raster_count(&r, 1, 14, 1, 35) == 0 && raster_count(&r, 13, 14, 13, 35) == 0); /* a pixel free each side */
    CHECK(raster_count(&r, 0, 40, 16, 63) == 0); /* no VOL fader under it */
    for (int k = 0; k < MM_MACROS; k++) {
        int cx = 33 + 26 * (k & 3), cy = k < 4 ? 21 : 48, ly = k < 4 ? 32 : 59;
        CHECK(r.px[cy - 8][cx] && r.px[cy + 8][cx]); /* the rim */
        CHECK(r.px[cy - 4][cx]);                /* the pointer, straight up at the centre */
        CHECK(raster_count(&r, cx - 2, ly, cx + 2, ly + 4) > 0); /* the letter */
    }
    s.m[0].pos = 63;
    raster_draw(&r, &s);
    CHECK(!r.px[21 - 4][33]);                   /* the pointer has turned right */
    CHECK(raster_count(&r, 34, 17, 40, 27) >= 4);

    s.m[1].pos = -64;
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 59 - 6, 17, 59 - 1, 27) >= 4); /* turned left */

    int before = raster_count(&r, 20, 32, 46, 36);
    s.popup = 1;                                /* a turned knob shows its value */
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 20, 32, 46, 36) != before);

    s.popup = 0;
    s.m[2].slot[1].dest = 30;                   /* a set knob shows its first destination */
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 85 - 12, 32, 85 + 12, 36) > 20);
}

static void setup_screen(void)
{
    mm_t s;
    memset(&s, 0, sizeof s);
    s.dests = &dests;
    s.state = MM_SETUP;
    s.edit = 2;
    s.sel = 1;
    s.m[2].pos = 40;
    s.m[2].slot[0] = (mm_slot){30, 40, 0x0005};
    s.m[2].slot[1] = (mm_slot){55, -12, 0xffff};
    raster_draw(&r, &s);
    CHECK(r.out_of_bounds == 0);
    CHECK(raster_count(&r, 30, 0, 98, 4) > 60); /* MODULATION SETUP */
    CHECK(raster_count(&r, 40, 6, 88, 10) > 40); /* MACRO MODE */
    for (int j = 0; j < MM_SLOTS; j++) {        /* the dotted separators */
        int x = 17 + 28 * j;
        CHECK(r.px[15][x] && !r.px[16][x] && r.px[63][x]);
        CHECK(r.px[40][31 + 28 * j] && r.px[56][31 + 28 * j]); /* the depth knob */
    }
    CHECK(r.px[23][46] && r.px[39][72]);        /* slot 2's heading is inverted */
    CHECK(!r.px[23][18] && !r.px[39][44] && !r.px[23][74]); /* the others are not */
    CHECK(raster_count(&r, 6, 26, 10, 30) > 4); /* the macro's letter */
    CHECK(r.px[40][3] && r.px[55][13] && r.px[56][1] && r.px[56][15]); /* its fader */
    CHECK(raster_count(&r, 6, 41, 10, 46) > 10 && raster_count(&r, 6, 49, 10, 54) == 0); /* filled up from the middle */
    s.m[2].pos = -64;
    raster_draw(&r, &s);
    CHECK(raster_count(&r, 6, 41, 10, 46) == 0 && raster_count(&r, 6, 49, 10, 54) > 10);
    s.sel = 3;
    raster_draw(&r, &s);
    CHECK(r.px[23][102] && r.px[39][127] && !r.px[23][46]);
}

void test_draw(int *checks_out, int *failures_out)
{
    nchecks = checks_out;
    nfail = failures_out;
    macro_screen();
    setup_screen();
}
