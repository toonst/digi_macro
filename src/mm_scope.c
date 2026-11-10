/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode, the kits feature: setups per project or per kit (docs/findings/s7-project-storage.md). Each kit keeps
 * its own setup and the project its own, whichever is on: changing the scope never overwrites the other. A setup
 * holds the knobs' positions too, so they are per kit (or per project) as the setup is: saved and loaded with it, and
 * taken along when the firmware copies the kit. elekloader's core saves and loads the blocks with the kits and the
 * project, and takes a kit's along when the firmware copies the kit (paste, undo, pattern copy); this keeps the block
 * on up to date and follows the kit on. A kit or a project with no setup yet gets a copy of the one on, its knobs at
 * the centre: its sound as saved. */
#include "mm.h"

/* core's calls: a kit's setup was loaded or copied, the project's was loaded. Any task. */
void mm_scope_kit_changed(mm_scope *p)
{
    p->gen++;
}

void mm_scope_proj_loaded(mm_scope *p)
{
    p->proj_loaded = 1;
}

static void keep(mm_scope *p, const mm_t *s)
{
    for (int i = 0; i < MM_MACROS; i++)
        p->kept[i] = s->m[i].pos;
}

static int knobs_moved(const mm_scope *p, const mm_t *s)
{
    for (int i = 0; i < MM_MACROS; i++)
        if (p->kept[i] != s->m[i].pos)
            return 1;
    return 0;
}

static uint8_t *block_on(mm_scope *p)
{
    if (p->mode == MM_SCOPE_PROJECT)
        return p->proj;
    return p->cur >= 0 && p->cur < MM_SCOPE_KITS ? p->kit[p->cur] : 0;
}

void mm_scope_edited(mm_scope *p, const mm_t *s)
{
    uint8_t *b = block_on(p);
    if (b) {
        mm_setup_pack(s, b);
        keep(p, s);
    }
}

/* none in this block yet: a copy of the setup on, the knobs at the centre */
static void fresh(mm_t *s, uint8_t *b)
{
    for (int i = 0; i < MM_MACROS; i++)
        if (s->m[i].pos) {
            s->m[i].pos = 0;
            s->dirty_mods = s->dirty_screen = 1;
        }
    mm_setup_pack(s, b);
}

/* the block's setup on, or a fresh one in it: 1 when the setup came from the block */
static int take(mm_t *s, uint8_t *b)
{
    if (mm_setup_valid(b))
        return mm_setup_unpack(s, b) == 0;
    fresh(s, b);
    return 0;
}

/* Per kit, the kit on's setup is on: again after a load or copy changed one. Per project, the project's: again after a
 * project load. */
int mm_scope_follow(mm_scope *p, mm_t *s, int kit)
{
    int changed = 0, moved = p->gen != p->seen, loaded = p->proj_loaded;
    p->seen = p->gen;
    p->proj_loaded = 0;
    if (p->mode == MM_SCOPE_PROJECT) {
        if (loaded || !mm_setup_valid(p->proj))
            changed = take(s, p->proj);
        p->cur = (int16_t)kit;
        keep(p, s);
        return changed;
    }
    if (loaded)
        p->cur = -1;                /* another project: the kit on's from it */
    if (kit < 0 || kit >= MM_SCOPE_KITS)
        return 0;
    if (kit != p->cur || moved) {
        changed = take(s, p->kit[kit]);
        p->cur = (int16_t)kit;
    }
    keep(p, s);
    return changed;
}

/* Each tick: an edit, or a knob turned, since the block on was written goes into it, unless a load or copy has
 * just changed that block (then the block wins: per project a project load, per kit any kit's load or copy); then
 * the kit on is followed. */
int mm_scope_tick(mm_scope *p, mm_t *s, int kit)
{
    int changed = p->proj_loaded || (p->mode == MM_SCOPE_KIT && p->gen != p->seen);
    if (!changed && (s->dirty_config || knobs_moved(p, s)))
        mm_scope_edited(p, s);
    return mm_scope_follow(p, s, kit);
}

/* Either way, the scope's own setup comes on, and one it has not got yet starts as a copy of the one on. */
void mm_scope_set(mm_scope *p, mm_t *s, int mode)
{
    if (mode == p->mode)
        return;
    mm_scope_edited(p, s);          /* the scope going off keeps the setup and knobs as they are */
    p->mode = (uint8_t)mode;
    if (mode == MM_SCOPE_PROJECT) {
        take(s, p->proj);
        keep(p, s);
    } else {
        p->cur = -1;                /* the next follow picks the kit's */
    }
    s->dirty_config = 1;
}
