/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The destinations a slot can take, per device (W4). PROVISIONAL until spike S6 reads the stock modulation
 * destination list: every per-track value-array index the frame carries (digikit finding 04: from 25, below the
 * modulation bound), minus the two routing switches. Names are the few digikit finding 02 has [D] (DT2 1.16, the same
 * on 1.17; DN2 1.11, the same on 1.12); the others show as P and the index. Machine parameters are named per
 * machine once S6 is done. */
#include "mm.h"

#if defined(MM_DT2)
#define FIRST 25
#define BOUND 70 /* the LFO DEST bound on the Digitakt II */
#define N (BOUND - FIRST - 1)
static uint8_t idx[N];
static const char *names[N];
static const struct { uint8_t i; const char *n; } known[] = {
    {55, "DEL SND"}, {56, "REV SND"}, {57, "CHO SND"}, {64, "BIT RED"}, {65, "SRR"}, {67, "OVERDRV"},
};
#define SKIP 66 /* SRR routing, a switch */
#elif defined(MM_DN2)
#define FIRST 25
#define BOUND 100 /* the LFO DEST bound on the Digitone II */
#define N (BOUND - FIRST - 1)
static uint8_t idx[N];
static const char *names[N];
static const struct { uint8_t i; const char *n; } known[] = {
    {86, "DEL SND"}, {87, "REV SND"}, {88, "CHO SND"}, {95, "BIT RED"}, {96, "SRR"}, {98, "OVERDRV"},
};
#define SKIP 97
#else
#error "MM_DT2 or MM_DN2"
#endif

static mm_dests dests;

/* Filled at run time (on the first event), so the table costs no image bytes. */
const mm_dests *mm_device_dests(void)
{
    if (!dests.count) {
        int n = 0;
        for (int i = FIRST; i < BOUND; i++) {
            if (i == SKIP)
                continue;
            idx[n] = (uint8_t)i;
            names[n] = 0;
            for (unsigned k = 0; k < sizeof known / sizeof known[0]; k++)
                if (known[k].i == i)
                    names[n] = known[k].n;
            n++;
        }
        dests.index = idx;
        dests.name = names;
        dests.bound = BOUND;
        dests.count = (uint8_t)n;
    }
    return &dests;
}
