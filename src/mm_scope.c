/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode, the kits feature: setups per project or per kit (docs/findings/s7-project-storage.md). Each kit keeps
 * its own setup and the project its own, whichever is on: changing the scope never overwrites the other.
 * elekloader's core saves and loads them with the kits and the project, and takes a kit's along when the firmware
 * copies the kit (paste, undo, pattern copy); this follows the kit on. The knobs' positions are not part of a setup
 * (they are not saved), but they follow the scope as well: per kit each kit's own come on with it, per project the
 * project's. A kit that has none yet has its knobs at the centre: its sound as saved. */
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

/* the knobs' positions into to, or from from (0: the centre) */
static void keep_pos(int8_t *to, const mm_t *s)
{
    for (int i = 0; i < MM_MACROS; i++)
        to[i] = s->m[i].pos;
}

static void put_pos(mm_t *s, const int8_t *from)
{
    for (int i = 0; i < MM_MACROS; i++) {
        int8_t v = from ? from[i] : 0;
        if (s->m[i].pos != v) {
            s->m[i].pos = v;
            s->dirty_mods = s->dirty_screen = 1;
        }
    }
}

static void keep_kit_pos(mm_scope *p, const mm_t *s)
{
    if (p->cur < 0 || p->cur >= MM_KITS)
        return;
    keep_pos(p->pos[p->cur], s);
    p->pos_kept[p->cur >> 3] |= (uint8_t)(1u << (p->cur & 7));
}

static void put_kit_pos(mm_scope *p, mm_t *s, int kit)
{
    put_pos(s, p->pos_kept[kit >> 3] & (1u << (kit & 7)) ? p->pos[kit] : 0);
}

void mm_scope_edited(mm_scope *p, const mm_t *s)
{
    if (p->mode == MM_SCOPE_PROJECT)
        mm_setup_pack(s, p->proj);
    else if (p->cur >= 0)
        mm_setup_pack(s, p->kit[p->cur]);
}

/* Per kit, the kit on's setup is on: again after a load or copy changed it. Per project, the project's: again after
 * a project load. One that has none yet starts as a copy of the one on. */
int mm_scope_follow(mm_scope *p, mm_t *s, int kit)
{
    int changed = 0, moved = p->gen != p->seen;
    p->seen = p->gen;
    if (p->proj_loaded) {           /* another project: its kits' knobs start at the centre */
        for (unsigned i = 0; i < sizeof p->pos_kept; i++)
            p->pos_kept[i] = 0;
        if (p->mode == MM_SCOPE_KIT)
            p->cur = -1;            /* the kit on's setup and knobs again, the old ones not kept */
    }
    if (p->mode == MM_SCOPE_PROJECT) {
        if (p->proj_loaded && mm_setup_valid(p->proj))
            changed = mm_setup_unpack(s, p->proj) == 0;
        else if (!mm_setup_valid(p->proj))
            mm_setup_pack(s, p->proj);
        p->proj_loaded = 0;
        p->cur = (int16_t)kit;
        return changed;
    }
    p->proj_loaded = 0;
    if (kit < 0 || kit >= MM_KITS)
        return 0;
    if (kit != p->cur) {            /* another kit on: the knobs are its own */
        keep_kit_pos(p, s);
        put_kit_pos(p, s, kit);
    }
    if (kit != p->cur || moved) {
        if (mm_setup_valid(p->kit[kit]))
            changed = mm_setup_unpack(s, p->kit[kit]) == 0;
        else
            mm_setup_pack(s, p->kit[kit]);
        p->cur = (int16_t)kit;
    }
    return changed;
}

/* Either way, the scope's own setup comes on, and one it has not got yet starts as a copy of the one on. */
void mm_scope_set(mm_scope *p, mm_t *s, int mode)
{
    if (mode == p->mode)
        return;
    p->mode = (uint8_t)mode;
    if (mode == MM_SCOPE_PROJECT) {
        keep_kit_pos(p, s);         /* the kit's knobs stay its own; the project's come back */
        put_pos(s, p->proj_pos);
        if (mm_setup_valid(p->proj))
            mm_setup_unpack(s, p->proj);
        else
            mm_setup_pack(s, p->proj);
    } else {
        keep_pos(p->proj_pos, s);
        p->cur = -1;                /* the next follow picks the kit's setup and knobs */
    }
    s->dirty_config = 1;
}
