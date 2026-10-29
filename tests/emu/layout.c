/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Where mm_t's and mm_scope's fields lie in the device build, for tests/emu/scenario.py: tools/check.sh compiles this with the
 * ColdFire compiler and reads the absolute symbols MMOFF_* (the host's offsets differ: m68k aligns to 2 bytes). */
#include <stddef.h>

#include "../../src/mm.h"

#define OFF(name, value) __asm__(".globl MMOFF_" #name "\n.set MMOFF_" #name ", %c0" ::"i"(value))

void mm_layout(void)
{
    OFF(state, offsetof(mm_t, state));
    OFF(edit, offsetof(mm_t, edit));
    OFF(sel, offsetof(mm_t, sel));
    OFF(m, offsetof(mm_t, m));
    OFF(macro, sizeof(mm_macro));
    OFF(pos, offsetof(mm_macro, pos));
    OFF(slot, offsetof(mm_macro, slot));
    OFF(slot_size, sizeof(mm_slot));
    OFF(dest, offsetof(mm_slot, dest));
    OFF(depth, offsetof(mm_slot, depth));
    OFF(tracks, offsetof(mm_slot, tracks));
    OFF(list, offsetof(mm_t, list));
    OFF(name, offsetof(mm_macro, name));
    OFF(dirty_config, offsetof(mm_t, dirty_config));
    OFF(scope_kit, offsetof(mm_scope, kit));
    OFF(scope_proj, offsetof(mm_scope, proj));
    OFF(scope_mode, offsetof(mm_scope, mode));
    OFF(scope_cur, offsetof(mm_scope, cur));
    OFF(scope_conflicts, offsetof(mm_scope, conflicts));
    OFF(setup_size, MM_SETUP_SIZE);
}
