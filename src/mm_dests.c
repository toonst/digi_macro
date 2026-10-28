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

#elif defined(MM_DN2)
/* The Digitone II 1.12's, from its parameter table (0x401f83a4): the filter, amp, FX and portamento parameters are
 * the Digitakt II's, at index + 31. No source parameter: the four machines share none (index 25 is FM Tone's
 * algorithm, the others' tune). */
#define BOUND 100 /* the LFO DEST bound on the Digitone II */
static const mm_dest table[] = {
    {67, MM_G_FLTR, 0, 0, "FREQ"},
    {68, MM_G_FLTR, 0, 0, "RESO"},
    {69, MM_G_FLTR, 0, 0, "ENV"},
    {70, MM_G_FLTR, 0, 0, "DEL"},
    {71, MM_G_FLTR, 0, 0, "ATK"},
    {72, MM_G_FLTR, 0, 0, "DEC"},
    {73, MM_G_FLTR, 0, 0, "SUS"},
    {74, MM_G_FLTR, 0, 0, "REL"},
    {76, MM_G_FLTR, 0, 0, "BASE"},
    {77, MM_G_FLTR, 0, 0, "WDTH"},
    {79, MM_G_FLTR, 0, 0x6400, "KEY.T"},
    {80, MM_G_AMP, 0, 0, "DEL"},
    {81, MM_G_AMP, 0, 0, "ATK"},
    {82, MM_G_AMP, 0, 0, "HOLD"},
    {83, MM_G_AMP, 0, 0, "DEC"},
    {84, MM_G_AMP, 0, 0, "SUS"},
    {85, MM_G_AMP, 0, 0, "REL"},
    {89, MM_G_AMP, 0, 0, "PAN"},
    {90, MM_G_AMP, 0, 0, "VOL"},
    {86, MM_G_FX, 0, 0, "DEL"},
    {87, MM_G_FX, 0, 0, "REV"},
    {88, MM_G_FX, 0, 0, "CHR"},
    {95, MM_G_FX, 0, 0, "BR"},
    {96, MM_G_FX, 0, 0, "SRR"},
    {98, MM_G_FX, 0, 0, "OVER"},
    {93, MM_G_PORT, 0, 0, "PTIM"},
};
#endif

#if defined(MM_DT2) || defined(MM_DN2)
static const mm_dests dests = {table, sizeof table / sizeof table[0], BOUND};

const mm_dests *mm_device_dests(void)
{
    return &dests;
}
#else
#error "MM_DT2 or MM_DN2"
#endif
