/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: the macro math, the per-track lists, the frame-time apply (PLAN.md 4.6) and the LEDs (4.5). */
#include "mm.h"

int32_t mm_slot_delta(int pos, int depth)
{
    /* |pos * depth| <= 4096: at full turn and full depth the delta spans the parameter range. */
    int32_t pd = (int32_t)pos * depth;
    int32_t d = pd * MM_RAW_MAX;
    return d >= 0 ? d >> 12 : -((-d) >> 12);  /* symmetric: -p gives exactly -(p) */
}

void mm_build(const mm_t *s, mm_mods *out)
{
    int32_t sum[MM_MAX_PAIRS];
    out->active = 0;
    for (int t = 0; t < MM_TRACKS; t++) {
        mm_track_mods *tm = &out->t[t];
        int n = 0;
        uint16_t bit = (uint16_t)(1u << t);
        for (int k = 0; k < MM_MACROS; k++) {
            const mm_macro *m = &s->m[k];
            if (!m->pos)
                continue;
            for (int j = 0; j < MM_SLOTS; j++) {
                const mm_slot *sl = &m->slot[j];
                int i;
                if (!sl->dest || !sl->depth || !(sl->tracks & bit))
                    continue;
                if (s->dests && sl->dest >= s->dests->bound)
                    continue;
                for (i = 0; i < n && tm->dest[i] != sl->dest; i++)
                    ;
                if (i == n) {       /* slots that hit one destination are merged */
                    int p = mm_dest_pos(s->dests, sl->dest);
                    const mm_dest *dd = p >= 0 ? &s->dests->d[p] : 0;
                    tm->dest[n] = sl->dest;
                    tm->lo[n] = dd ? dd->lo : 0;
                    tm->hi[n] = dd && dd->hi ? dd->hi : MM_RAW_MAX;
                    sum[n++] = 0;
                }
                sum[i] += mm_slot_delta(m->pos, sl->depth);
            }
        }
        int kept = 0;
        for (int i = 0; i < n; i++) {
            int32_t d = sum[i] > MM_RAW_MAX ? MM_RAW_MAX : sum[i] < -MM_RAW_MAX ? -MM_RAW_MAX : sum[i];
            if (!d)
                continue;           /* two slots that cancel out */
            tm->dest[kept] = tm->dest[i];
            tm->lo[kept] = tm->lo[i];
            tm->hi[kept] = tm->hi[i];
            tm->delta[kept++] = (int16_t)d;
        }
        tm->n = (uint8_t)kept;
        if (kept)
            out->active |= bit;
    }
}

void mm_apply(const mm_mods *mods, int16_t *values, int stride, int bound)
{
    uint16_t active = mods->active;
    for (int t = 0; active; t++, active >>= 1) {
        if (!(active & 1))
            continue;
        const mm_track_mods *tm = &mods->t[t];
        int16_t *v = values + t * stride;
        for (int i = 0; i < tm->n; i++) {
            int d = tm->dest[i];
            if (d >= bound)
                continue;
            int32_t x = (int32_t)v[d] + tm->delta[i];
            v[d] = (int16_t)(x < tm->lo[i] ? tm->lo[i] : x > tm->hi[i] ? tm->hi[i] : x);
        }
    }
}

int mm_leds(const mm_t *s, uint8_t *code, uint8_t *value, int cap)
{
    int n = 0;
    if (s->state == MM_OFF || cap < 1)
        return 0;
    code[n] = MM_K_MOD;
    value[n++] = MM_LED_BLUE;
    for (int k = MM_K_TRIG; k <= MM_K_FX && n < cap; k++) { /* the page macro mode came from no longer shows lit */
        code[n] = (uint8_t)k;
        value[n++] = MM_LED_OFF;
    }
    if (s->state == MM_SETUP) {     /* the selected slot's tracks, red; the playhead does not show */
        uint16_t tracks = s->m[s->edit].slot[s->sel].tracks;
        for (int t = 0; t < MM_TRACKS && n < cap; t++) {
            code[n] = (uint8_t)(MM_K_TRIG1 + t);
            value[n++] = (tracks >> t) & 1 ? MM_LED_RED : MM_LED_OFF;
        }
    }
    return n;
}
