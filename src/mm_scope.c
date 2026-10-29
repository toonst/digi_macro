/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode, the kits feature: setups per project or per kit (docs/findings/s7-project-storage.md). Each kit keeps
 * its own setup in its saved record, and the project keeps its own in the saved project's header, whichever is on:
 * changing the scope never overwrites the other. The firmware's copies of kits in RAM (paste, undo, pattern copy)
 * take the setups along. The saved areas are shared with the stock OS, so the mod writes and reads only its own setups
 * there and counts anything else as a conflict. Positions are not part of a setup: the knobs stay where they are when the setup changes. */
#include "mm.h"

static void copy(uint8_t *to, const uint8_t *from)
{
    for (int i = 0; i < MM_SETUP_SIZE; i++)
        to[i] = from[i];
}

static void put(uint8_t *out, const uint8_t *from)
{
    if (mm_setup_valid(from))
        copy(out, from);
    else
        for (int i = 0; i < MM_SETUP_SIZE; i++)
            out[i] = 0;             /* none: the bytes the stock save leaves there */
}

/* Into a saved record: over zeros or a setup of this mod's only. */
static void save(mm_scope *p, uint8_t *out, const uint8_t *from)
{
    if (!mm_setup_blank(out) && !mm_setup_valid(out)) {
        p->conflicts++;
        return;
    }
    put(out, from);
}

/* From a saved record: zeros are none, a setup of this mod's is taken, anything else is not this mod's. */
static void load(mm_scope *p, uint8_t *to, const uint8_t *in)
{
    if (!mm_setup_blank(in) && !mm_setup_valid(in))
        p->conflicts++;
    put(to, in);
}

void mm_scope_kit_save(mm_scope *p, int kit, uint8_t *out)
{
    static const uint8_t none[MM_SETUP_SIZE];
    save(p, out, kit >= 0 && kit < MM_KITS ? p->kit[kit] : none);
}

void mm_scope_kit_load(mm_scope *p, int kit, const uint8_t *in)
{
    if (kit < 0 || kit >= MM_KITS)
        return;
    load(p, p->kit[kit], in);       /* a kit from before the mod, or saved without it: none */
    p->gen++;
}

void mm_scope_proj_save(mm_scope *p, uint8_t *out)
{
    save(p, out, p->proj);
}

void mm_scope_proj_load(mm_scope *p, const uint8_t *in)
{
    load(p, p->proj, in);
    p->proj_loaded = 1;
}

static mm_shadow *shadow(mm_scope *p, uint32_t addr)
{
    for (int i = 0; i < MM_SHADOWS; i++)
        if (p->shadow[i].addr == addr)
            return &p->shadow[i];
    return 0;
}

/* A kit copied in RAM, from src to dst (kit numbers -1: not one of the project's kits, an other buffer: the
 * clipboard, the undo copy). A buffer holding a kit keeps a shadow of its setup, so a paste from it brings the
 * setup back. A copy from a buffer the mod never saw filled leaves the target's setup as it is. */
void mm_scope_copied(mm_scope *p, int dst_kit, uint32_t dst, int src_kit, uint32_t src)
{
    const uint8_t *from = 0;
    mm_shadow *sh;
    if (dst == src)
        return;
    if (src_kit >= 0 && src_kit < MM_KITS)
        from = p->kit[src_kit];
    else if ((sh = shadow(p, src)) != 0)
        from = sh->setup;
    if (dst_kit >= 0 && dst_kit < MM_KITS) {
        if (from) {
            put(p->kit[dst_kit], from);
            p->gen++;
        }
        return;
    }
    sh = shadow(p, dst);
    if (!from) {
        if (sh)
            sh->addr = 0;           /* that buffer holds a kit the mod knows nothing of now */
        return;
    }
    if (!sh) {
        sh = &p->shadow[p->shadow_next];
        p->shadow_next = (uint8_t)((p->shadow_next + 1) % MM_SHADOWS);
    }
    sh->addr = dst;
    put(sh->setup, from);
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
