/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: setups per project or per pattern, kept in the project's own records (docs/findings/s7-project-
 * storage.md). The firmware's save writes, per pattern, the setup that pattern has (or, per project, the project's)
 * into bytes of the saved record it leaves unused; its load reads them back. Positions are not part of a setup: the
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

void mm_scope_save(const mm_scope *p, int pattern, uint8_t *out)
{
    const uint8_t *from = 0;
    if (p->mode == MM_SCOPE_PROJECT)
        from = p->proj;
    else if (pattern >= 0 && pattern < MM_PATTERNS)
        from = p->pat[pattern];
    if (from && mm_setup_valid(from))
        copy(out, from);
    else
        clear(out);                 /* none: the bytes the stock save leaves there */
}

void mm_scope_load(mm_scope *p, int pattern, const uint8_t *in)
{
    int ok = mm_setup_valid(in);
    if (pattern < 0 || pattern >= MM_PATTERNS)
        return;
    if (ok)
        copy(p->pat[pattern], in);
    else
        clear(p->pat[pattern]);     /* a project from before the mod, or saved without it */
    if (ok && in[2] & MM_SETUP_PROJECT) {
        copy(p->proj, in);
        p->proj_loaded = 1;
    }
    p->loads++;
}

void mm_scope_edited(mm_scope *p, const mm_t *s)
{
    if (p->mode == MM_SCOPE_PROJECT)
        mm_setup_pack(s, MM_SETUP_PROJECT, p->proj);
    else if (p->cur >= 0)
        mm_setup_pack(s, 0, p->pat[p->cur]);
}

/* A setup the pattern or project has not got yet starts as a copy of the one on. */
int mm_scope_follow(mm_scope *p, mm_t *s, int pattern)
{
    int changed = 0, loaded = p->loads != p->seen;
    p->seen = p->loads;
    if (p->mode == MM_SCOPE_PROJECT) {
        if (loaded && p->proj_loaded)
            changed = mm_setup_unpack(s, p->proj) >= 0;
        else if (loaded || !mm_setup_valid(p->proj))
            mm_setup_pack(s, MM_SETUP_PROJECT, p->proj);
        p->proj_loaded = 0;
        p->cur = (int16_t)pattern;
        return changed;
    }
    p->proj_loaded = 0;
    if (pattern < 0 || pattern >= MM_PATTERNS)
        return 0;
    if (pattern != p->cur || loaded) {
        if (mm_setup_valid(p->pat[pattern]))
            changed = mm_setup_unpack(s, p->pat[pattern]) >= 0;
        else
            mm_setup_pack(s, 0, p->pat[pattern]);
        p->cur = (int16_t)pattern;
    }
    return changed;
}

/* Per project from per pattern: the setup on becomes the project's. Per pattern: the pattern's own, if it has one. */
void mm_scope_set(mm_scope *p, mm_t *s, int mode)
{
    if (mode == p->mode)
        return;
    p->mode = (uint8_t)mode;
    if (mode == MM_SCOPE_PROJECT)
        mm_setup_pack(s, MM_SETUP_PROJECT, p->proj);
    else
        p->cur = -1;                /* the next follow picks the pattern's setup */
    s->dirty_config = 1;
}
