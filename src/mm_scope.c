/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: setups per project or per kit, kept in the project's kit records (docs/findings/s7-project-
 * storage.md). The firmware's save of a kit record writes the setup that kit has (or, per project, the project's)
 * into bytes of the record it leaves unused; its load of one reads it back. Positions are not part of a setup: the
 * knobs stay where they are when the setup changes. */
#include "mm.h"

static void copy(uint8_t *to, const uint8_t *from)
{
    for (int i = 0; i < MM_SETUP_SIZE; i++)
        to[i] = from[i];
}

static void clear(uint8_t *to)
{
    for (int i = 0; i < MM_SETUP_SIZE; i++)
        to[i] = 0;
}

void mm_scope_save(const mm_scope *p, int kit, uint8_t *out)
{
    const uint8_t *from = 0;
    if (p->mode == MM_SCOPE_PROJECT)
        from = p->proj;
    else if (kit >= 0 && kit < MM_KITS)
        from = p->kit[kit];
    if (from && mm_setup_valid(from))
        copy(out, from);
    else
        clear(out);                 /* none: the bytes the stock save leaves there */
}

void mm_scope_load(mm_scope *p, int kit, const uint8_t *in)
{
    if (kit < 0 || kit >= MM_KITS)
        return;
    if (mm_setup_valid(in))
        copy(p->kit[kit], in);
    else
        clear(p->kit[kit]);         /* a kit from before the mod, or saved without it */
    p->loads++;
}

void mm_scope_edited(mm_scope *p, const mm_t *s)
{
    if (p->mode == MM_SCOPE_PROJECT)
        mm_setup_pack(s, p->proj);
    else if (p->cur >= 0)
        mm_setup_pack(s, p->kit[p->cur]);
}

/* Loads come in bursts: a project load brings every kit, a kit manager load one. A burst ends at the first tick
 * without loads. Per kit, the kit on takes its setup again after any; per project, a project load brings the setup
 * of the kit on (all of them carry the project's when it was saved per project). A setup a kit or the project has
 * not got yet starts as a copy of the one on. */
int mm_scope_follow(mm_scope *p, mm_t *s, int kit)
{
    int changed = 0, n = (uint8_t)(p->loads - p->seen), ended = 0, project_load = 0;
    p->seen = p->loads;
    if (n) {
        p->burst = (uint8_t)(p->burst + n > 255 ? 255 : p->burst + n);
    } else if (p->burst) {
        ended = 1;
        project_load = p->burst >= MM_KITS;
        p->burst = 0;
    }
    if (p->mode == MM_SCOPE_PROJECT) {
        if (project_load && kit >= 0 && kit < MM_KITS && mm_setup_valid(p->kit[kit])) {
            changed = mm_setup_unpack(s, p->kit[kit]) == 0;
            copy(p->proj, p->kit[kit]);
        } else if (project_load || !mm_setup_valid(p->proj)) {
            mm_setup_pack(s, p->proj);
        }
        p->cur = (int16_t)kit;
        return changed;
    }
    if (kit < 0 || kit >= MM_KITS || p->burst)
        return 0;                   /* no kit known, or loads still coming */
    if (kit != p->cur || ended) {
        if (mm_setup_valid(p->kit[kit]))
            changed = mm_setup_unpack(s, p->kit[kit]) == 0;
        else
            mm_setup_pack(s, p->kit[kit]);
        p->cur = (int16_t)kit;
    }
    return changed;
}

/* Per project from per kit: the setup on becomes the project's. Per kit: the kit's own, if it has one. */
void mm_scope_set(mm_scope *p, mm_t *s, int mode)
{
    if (mode == p->mode)
        return;
    p->mode = (uint8_t)mode;
    if (mode == MM_SCOPE_PROJECT)
        mm_setup_pack(s, p->proj);
    else
        p->cur = -1;                /* the next follow picks the kit's setup */
    s->dirty_config = 1;
}
