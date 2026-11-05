/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The firmware locations mm_glue.c uses, per device and OS: the Digitakt II's OS 1.17 (MM_DT2) and the Digitone II's
 * OS 1.12 (MM_DN2). Addresses and sizes only, no firmware bytes; docs/findings has what each is. They were mod.json's
 * cflags; here, a manifest needs only the device's define, as builders that take a short list of compiler options
 * (Modwerk's) do. */
#ifndef MM_OS_H
#define MM_OS_H

#if defined(MM_DT2)
#define FILLRECT_AT     0x401131ae
#define LEDSET_AT       0x400308e0
#define CARD_READ_AT    0x4012e1b6
#define CARD_WRITE_AT   0x4012e39c
#define CARD_SECTOR     0x2df0e00
#define VIEW_TOP_BASE   0x40216f14
#define VIEW_FUNC       0x401f9208
#define TEXT_AT         0x40113dc8
#define TEXT_FONT       0x44d2e70c
#define TEXT_FONT_SMALL 0x44d2e6f4
#define KEY_DOWN_AT     0x4011e150
#define ENC_BLOCK_AT    0x40111b68
#define ENC_BLOCK_SIZE  252
#define ENC_FILTER_AT   0x40111cda
#define ENC_LIST_AT     0x40111e42
#define ENC_CFG_LIST    0x44d2e600
#define ENC_CFG_PARAM   0x44d2e654
#define ENC_CFG_DEPTH   0x44d2e670
#define HEADER_OBJ_AT   0x401998c0
#define HEADER_MSG_AT   0x4005b23a
#define OP_NEW_AT       0x4011ebd0
#define STR_NEW_AT      0x401e7e7a
#define STR_FREE_AT     0x401e63ca
#define NAME_VIEW_AT    0x40117c70
#define VIEW_PUSH_AT    0x4011c08a
#define SP_RELEASE_AT   0x40197d26
#define SP_INPLACE_VT   0x401ef190
#elif defined(MM_DN2)
#define FILLRECT_AT     0x40114b22
#define LEDSET_AT       0x4002c318
#define CARD_READ_AT    0x4012c87a
#define CARD_WRITE_AT   0x4012ca60
#define CARD_SECTOR     0x7ff000
#define VIEW_TOP_BASE   0x402008e8
#define VIEW_FUNC       0x401e1eb4
#define TEXT_AT         0x4011573c
#define TEXT_FONT       0x44508ef8
#define TEXT_FONT_SMALL 0x44508ee0
#define KEY_DOWN_AT     0x4011fac4
#define ENC_BLOCK_AT    0x401134dc
#define ENC_BLOCK_SIZE  252
#define ENC_FILTER_AT   0x4011364e
#define ENC_LIST_AT     0x401137b6
#define ENC_CFG_LIST    0x44508dec
#define ENC_CFG_PARAM   0x44508e40
#define ENC_CFG_DEPTH   0x44508e5c
#define HEADER_OBJ_AT   0x4018b2a4
#define HEADER_MSG_AT   0x400562e6
#define OP_NEW_AT       0x40120544
#define STR_NEW_AT      0x401ceab8
#define STR_FREE_AT     0x401cd008
#define NAME_VIEW_AT    0x401195e4
#define VIEW_PUSH_AT    0x4011d9fe
#define SP_RELEASE_AT   0x401883ac
#define SP_INPLACE_VT   0x401e0fd8
#else
#error "MM_DT2 or MM_DN2"
#endif

#endif
