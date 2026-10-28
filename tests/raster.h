/* SPDX-License-Identifier: GPL-2.0-or-later */
/* A 128 x 64 host canvas for the tests and the previews: y = 0 is the top row, as mm_canvas wants. */
#ifndef RASTER_H
#define RASTER_H
#include <string.h>

#include "../src/mm.h"

typedef struct {
    unsigned char px[64][128];
    int calls, out_of_bounds;
} raster;

static inline void raster_fill(void *ctx, int x0, int y0, int x1, int y1, int colour)
{
    raster *r = ctx;
    r->calls++;
    if (x0 < 0 || y0 < 0 || x1 > 127 || y1 > 63 || x0 > x1 || y0 > y1) {
        r->out_of_bounds++;
        return;
    }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            r->px[y][x] = colour < 0 ? !r->px[y][x] : colour != 0;
}

static inline void raster_draw(raster *r, const mm_t *s)
{
    mm_canvas c = {raster_fill, r};
    memset(r, 0, sizeof *r);
    mm_draw(s, &c);
}

static inline int raster_count(const raster *r, int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            n += r->px[y][x];
    return n;
}
#endif
