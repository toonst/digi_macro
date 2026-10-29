/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode, the kits feature on the device (src/mm_kits.h): the setups as elekloader core's kit and project data,
 * the kit on, and SETTINGS > PERSONALIZE > MACRO SETUP (core-dt2 1.1, core-dn2 0.3: elekloader's docs/ADAPTING.md,
 * "Frames, LEDs, project and kit data, value rows"). The logic is mm_scope.c's. */
#include "mm_kits.h"
#include "mk2/core.h"

/* Not static, for the emulator scenarios (tests/emu/project.py). */
mm_scope mm_scopes;

static void kit_changed(int kit, int found)
{
    (void)kit, (void)found;         /* none found: core zeroed the block, which is none here too */
    mm_scope_kit_changed(&mm_scopes);
}

static void proj_loaded(int found)
{
    (void)found;
    mm_scope_proj_loaded(&mm_scopes);
}

/* contributed to core's tables (mods/<dev>/kits.json), claimed as kitdata:MMKS and projdata:MMPS */
const core_kitdata_t mm_kitdata = {{'M', 'M', 'K', 'S'}, MM_SETUP_SIZE, mm_scopes.kit, kit_changed};
const core_projdata_t mm_projdata = {{'M', 'M', 'P', 'S'}, MM_PROJ_SIZE, mm_scopes.proj, proj_loaded};

void mm_kits_tick(mm_t *s)
{
    static uint8_t ready;
    static uint32_t conflicts;
    if (!ready) {
        mm_scopes.cur = -1;
        conflicts = core_store_conflicts;
        ready = 1;
    }
    if (s->dirty_config)            /* the setup the kit or project has, as it is now */
        mm_scope_edited(&mm_scopes, s);
    mm_scope_follow(&mm_scopes, s, core_kit_on());
    if (core_store_conflicts != conflicts) { /* a project's bytes for mods held something else: core left them */
        conflicts = core_store_conflicts;
        mm_notice("MACRO SETUP CONFLICT");
    }
}

/* The card's record keeps the scope: 0x80 | MM_SCOPE_*; older records have 0, per project. */
uint8_t mm_kits_card(void)
{
    return (uint8_t)(0x80 | mm_scopes.mode);
}

void mm_kits_card_restore(uint8_t b)
{
    if (b & 0x80)
        mm_scopes.mode = b & 1;
}

/* SETTINGS > PERSONALIZE > MACRO SETUP: PROJECT or KIT */
static const char *const scope_names[] = {"PROJECT", "KIT"};

static uint32_t row_get(void)
{
    return mm_scopes.mode;
}

static void row_set(uint32_t mode)
{
    mm_scope_set(&mm_scopes, mm_get(), (int)mode);
}

static const core_value_t mm_row = {"MACRO SETUP", scope_names, 2, row_get, row_set};

void mm_ev_personalize(void *menu)
{
    core_addvalue(menu, &mm_row);
}
