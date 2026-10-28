/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Writes the screens as PBM files into DIR (tools/check.sh turns them into PNGs in docs/img). */
#include <stdio.h>

#include "raster.h"

static const mm_dest table[] = {
    {25, MM_G_SRC, 0x400, 0x7c00, "TUNE", "Tune"}, {36, MM_G_FLTR, 0, 0, "FREQ", "Frequency"},
    {37, MM_G_FLTR, 0, 0, "RESO", "Resonance"}, {55, MM_G_FX, 0, 0, "DEL", "Delay Send"},
};
static const mm_dests dests = {table, 4, 70};

static void save(const raster *r, const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.pbm", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P1\n128 64\n");
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++)
            fputc(r->px[y][x] ? '1' : '0', f);
        fputc('\n', f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    static raster r;
    mm_t s;
    const char *dir = argc > 1 ? argv[1] : ".";
    memset(&s, 0, sizeof s);
    s.dests = &dests;

    s.state = MM_MACRO;
    raster_draw(&r, &s);
    save(&r, dir, "macro-centre");

    s.m[0].pos = 40;
    s.m[1].pos = -25;
    s.m[2].pos = 63;
    s.m[2].slot[0] = (mm_slot){36, 40 * 512, 0x0005};
    s.m[5].pos = -64;
    mm_set_name(&s, 3, "BASS");                 /* a named macro */
    s.popup = 2;
    raster_draw(&r, &s);
    save(&r, dir, "macro-turned");

    s.state = MM_SETUP;
    s.edit = 2;
    s.sel = 1;
    s.m[2].slot[1] = (mm_slot){55, -12 * 512, 0x0014};
    s.m[2].slot[2] = (mm_slot){25, 5 * 512 + 77, 0xffff};
    raster_draw(&r, &s);
    save(&r, dir, "setup");

    s.list = 2;                                 /* B turned: slot 2's destination list */
    s.list_top = 0;
    raster_draw(&r, &s);
    save(&r, dir, "setup-list");
    return 0;
}
