/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode, the kits feature: setups per project or per kit (docs/findings/s7-project-storage.md). Each kit keeps
 * its own setup and the project its own, whichever is on: changing the scope never overwrites the other.
 * elekloader's core saves and loads them with the kits and the project, and takes a kit's along when the firmware
 * copies the kit (paste, undo, pattern copy); this follows the kit on. Positions are not part of a setup: the knobs
 * stay where they are when the setup changes. */
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
        if (mm_setup_valid(p->proj))
            mm_setup_unpack(s, p->proj);
        else
            mm_setup_pack(s, p->proj);
    } else {
        p->cur = -1;                /* the next follow picks the kit's */
    }
    s->dirty_config = 1;
}
