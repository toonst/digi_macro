/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The destinations a slot can take, per device (W4, spike S6; docs/findings/s6-destinations.md). */
#include "mm.h"

#if defined(MM_DT2)
/* The Digitakt II 1.17's per-track continuous parameters that every machine shares, in page order, with the short
 * names and ranges of the firmware's parameter table (0x4020f59c). Left out: switches (play mode, sample slot and bank,
 * resets, routings, amp mode, portamento on), the filter type, and the source parameters 27 and 31 .. 33, whose
 * meaning and range change with the machine. */
#define BOUND 70 /* the LFO DEST bound on the Digitakt II */
static const mm_dest table[] = {
    {25, MM_G_SRC, 0x400, 0x7c00, "TUNE"},
    {34, MM_G_SRC, 0, 0, "LEV"},
    {36, MM_G_FLTR, 0, 0, "FREQ"},
    {37, MM_G_FLTR, 0, 0, "RESO"},
    {38, MM_G_FLTR, 0, 0, "ENV"},
    {39, MM_G_FLTR, 0, 0, "DEL"},
    {40, MM_G_FLTR, 0, 0, "ATK"},
    {41, MM_G_FLTR, 0, 0, "DEC"},
    {42, MM_G_FLTR, 0, 0, "SUS"},
    {43, MM_G_FLTR, 0, 0, "REL"},
    {45, MM_G_FLTR, 0, 0, "BASE"},
    {46, MM_G_FLTR, 0, 0, "WDTH"},
    {48, MM_G_FLTR, 0, 0x6400, "KEY.T"},
    {49, MM_G_AMP, 0, 0, "DEL"},
    {50, MM_G_AMP, 0, 0, "ATK"},
    {51, MM_G_AMP, 0, 0, "HOLD"},
    {52, MM_G_AMP, 0, 0, "DEC"},
    {53, MM_G_AMP, 0, 0, "SUS"},
    {54, MM_G_AMP, 0, 0, "REL"},
    {58, MM_G_AMP, 0, 0, "PAN"},
    {59, MM_G_AMP, 0, 0, "VOL"},
    {55, MM_G_FX, 0, 0, "DEL"},
    {56, MM_G_FX, 0, 0, "REV"},
    {57, MM_G_FX, 0, 0, "CHR"},
    {64, MM_G_FX, 0, 0, "BR"},
    {65, MM_G_FX, 0, 0, "SRR"},
    {67, MM_G_FX, 0, 0, "OVER"},
    {62, MM_G_PORT, 0, 0, "PTIM"},
};
static const mm_dests dests = {table, sizeof table / sizeof table[0], BOUND};

const mm_dests *mm_device_dests(void)
{
    return &dests;
}

#elif defined(MM_DN2)
/* PROVISIONAL until spike S6 is done on the Digitone II 1.12: every per-track value-array index the frame carries
 * (digikit finding 04: from 25, below the modulation bound), minus the SRR routing switch; the few names digikit
 * finding 02 has, the others shown as P and the index. */
#define FIRST 25
#define BOUND 100 /* the LFO DEST bound on the Digitone II */
#define SKIP 97
#define N (BOUND - FIRST - 1)
static mm_dest table[N];
static const struct { uint8_t i; uint8_t g; const char *n; } known[] = {
    {86, MM_G_FX, "DEL"}, {87, MM_G_FX, "REV"}, {88, MM_G_FX, "CHR"},
    {95, MM_G_FX, "BR"},  {96, MM_G_FX, "SRR"}, {98, MM_G_FX, "OVER"},
};
static mm_dests dests;

/* Filled at run time (on the first event), so the table costs no image bytes. */
const mm_dests *mm_device_dests(void)
{
    if (!dests.count) {
        int n = 0;
        for (int i = FIRST; i < BOUND; i++) {
            if (i == SKIP)
                continue;
            table[n] = (mm_dest){(uint8_t)i, MM_G_NONE, 0, 0, 0};
            for (unsigned k = 0; k < sizeof known / sizeof known[0]; k++)
                if (known[k].i == i)
                    table[n].group = known[k].g, table[n].name = known[k].n;
            n++;
        }
        dests.d = table;
        dests.bound = BOUND;
        dests.count = (uint8_t)n;
    }
    return &dests;
}
#else
#error "MM_DT2 or MM_DN2"
#endif
