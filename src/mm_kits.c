/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode, the kits feature on the device (src/mm_kits.h): the firmware's calls (src/kit.s), the kit on, and
 * SETTINGS > PERSONALIZE > MACRO SETUP. The device's addresses come from mods/<dev>/kits.json's cflags; the logic is
 * mm_scope.c's. */
#include "mm_kits.h"

/* Not static, for the emulator scenarios (tests/emu/project.py). */
mm_scope mm_scopes;

/* The project's kits are a table (KIT_RAM, KIT_STRIDE; kit n is pattern n's): a kit's RAM gives its number, -1 for
 * one outside it. */
static int kit_index(const uint8_t *ram)
{
    uint32_t off = (uint32_t)ram - KIT_RAM;
    if ((uint32_t)ram < KIT_RAM || off % KIT_STRIDE || off / KIT_STRIDE >= MM_KITS)
        return -1;
    return (int)(off / KIT_STRIDE);
}

/* A kit record's save and load: the setup at KIT_PAD, after its end mark, where the firmware writes nothing. */
void mm_kit_save(uint8_t *record, const uint8_t *ram)
{
    mm_scope_kit_save(&mm_scopes, kit_index(ram), record + KIT_PAD);
}

void mm_kit_load(uint8_t *a2, uint8_t *a3)
{
#ifdef KIT_LOAD_RAM_A3
    uint8_t *ram = a3, *record = a2;
#else
    uint8_t *ram = a2, *record = a3;
#endif
    mm_scope_kit_load(&mm_scopes, kit_index(ram), record + KIT_PAD);
}

/* The project's header: the project's setup at PROJ_PAD, after the name and the fields the firmware keeps there. */
void mm_proj_save(uint8_t *file)
{
    mm_scope_proj_save(&mm_scopes, file + PROJ_PAD);
}

void mm_proj_load(const uint8_t *file)
{
    mm_scope_proj_load(&mm_scopes, file + PROJ_PAD);
}

/* memcpy of a kit's size (KIT_SIZE): paste, undo, pattern copy. */
void mm_kit_copied(uint8_t *dst, const uint8_t *src)
{
    mm_scope_copied(&mm_scopes, kit_index(dst), (uint32_t)dst, kit_index(src), (uint32_t)src);
}

/* The kit on is the pattern on's: the sequencer object's (PATTERN_OBJ, the pointer its getter returns) + 0x138, the
 * same on both. */
static int fw_kit(void)
{
    uint32_t o = *(volatile uint32_t *)PATTERN_OBJ, n;
    if (!o)
        return -1;
    n = *(volatile uint32_t *)(o + 0x138);
    return n < MM_KITS ? (int)n : -1;
}

void mm_kits_tick(mm_t *s)
{
    static uint8_t ready;
    if (!ready) {
        mm_scopes.cur = -1;
        ready = 1;
    }
    if (s->dirty_config)            /* the setup the kit or project has, as it is now */
        mm_scope_edited(&mm_scopes, s);
    mm_scope_follow(&mm_scopes, s, fw_kit());
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

/* SETTINGS > PERSONALIZE > MACRO SETUP: PROJECT or KIT, a value row as the stock LED INTENSITY (elekloader's
 * core_additem; label in src/rows.s). Each callback gets the std::function's storage, whose first word is the menu;
 * the value is drawn as LED INTENSITY's (the firmware's text routine at the row's x + 19, y + 2, right-aligned), and a
 * change redraws the menu with View::invalidate(menu + 0x38). */
extern void core_additem(void *menu, const void *row);
extern void mm_row_label(void);

static void row_set(void **storage, int mode)
{
    mm_scope_set(&mm_scopes, mm_get(), mode);
    ((void (*)(void *))MENU_INVALIDATE_AT)((char *)*storage + 0x38);
}

static void row_select(void **storage)
{
    row_set(storage, mm_scopes.mode ^ 1);
}

static void row_draw(void *storage, void *item, void *bmp, int x, int y)
{
    typedef void (*row_text_t)(void *bmp, const void *font, int x, int y, int align, int a, int b, const char *s);
    (void)storage, (void)item;
    ((row_text_t)ROW_TEXT_AT)(bmp, (const void *)ROW_FONT, x + 19, y + 2, 2, 0, 0,
                              mm_scopes.mode == MM_SCOPE_KIT ? "KIT" : "PROJECT");
}

static void row_change(void **storage, int item, int delta)
{
    (void)item;
    if (delta)
        row_set(storage, delta > 0 ? MM_SCOPE_KIT : MM_SCOPE_PROJECT);
}

static const void *const mm_row[4] = {
    (const void *)mm_row_label, (const void *)row_select, (const void *)row_draw, (const void *)row_change,
};

void mm_ev_personalize(void *menu)
{
    core_additem(menu, mm_row);
}
