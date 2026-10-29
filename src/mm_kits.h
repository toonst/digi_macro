/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The kits feature's interface to the glue (mm_glue.c): setups per project or per kit, in the project's own data
 * (src/mm_kits.c, src/mm_scope.c, src/kit.s, src/rows.s; docs/findings/s7-project-storage.md). A build without it
 * (mods/<dev>/kits.json left out, so no MM_FEATURE_KITS) gets these empty, and the mode keeps one global setup. */
#ifndef MM_KITS_H
#define MM_KITS_H

#include "mm.h"

mm_t *mm_get(void); /* the mode's state (mm_glue.c) */

#ifdef MM_FEATURE_KITS
void mm_kits_tick(mm_t *s);           /* each tick, after the card's configuration is in */
uint8_t mm_kits_card(void);           /* the byte the card's record keeps for it (MM_RECORD_SCOPE) */
void mm_kits_card_restore(uint8_t b);
#else
static inline void mm_kits_tick(mm_t *s)
{
    (void)s;
}
static inline uint8_t mm_kits_card(void)
{
    return 0;
}
static inline void mm_kits_card_restore(uint8_t b)
{
    (void)b;
}
#endif

#endif
