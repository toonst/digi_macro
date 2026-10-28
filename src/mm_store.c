/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: the stored configuration (PLAN.md 4.7) and the destination list (W4). */
#include "mm.h"

/* "MM", version, macros, slots; then per slot dest, depth, tracks (big-endian); then a CRC-16/CCITT of all before. */
static uint16_t crc16(const uint8_t *p, int n)
{
    uint16_t c = 0xffff;
    while (n--) {
        c ^= (uint16_t)(*p++ << 8);
        for (int b = 0; b < 8; b++)
            c = (uint16_t)(c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}

int mm_encode(const mm_t *s, uint8_t *out)
{
    int o = 0;
    out[o++] = 'M';
    out[o++] = 'M';
    out[o++] = MM_BLOB_VERSION;
    out[o++] = MM_MACROS;
    out[o++] = MM_SLOTS;
    for (int k = 0; k < MM_MACROS; k++)
        for (int j = 0; j < MM_SLOTS; j++) {
            const mm_slot *sl = &s->m[k].slot[j];
            out[o++] = sl->dest;
            out[o++] = (uint8_t)sl->depth;
            out[o++] = (uint8_t)(sl->tracks >> 8);
            out[o++] = (uint8_t)sl->tracks;
        }
    uint16_t c = crc16(out, o);
    out[o++] = (uint8_t)(c >> 8);
    out[o++] = (uint8_t)c;
    return o;
}

int mm_decode(mm_t *s, const uint8_t *in, int len)
{
    if (len != MM_BLOB_SIZE || in[0] != 'M' || in[1] != 'M' || in[2] != MM_BLOB_VERSION || in[3] != MM_MACROS ||
        in[4] != MM_SLOTS)
        return -1;
    if (crc16(in, len - 2) != (uint16_t)(in[len - 2] << 8 | in[len - 1]))
        return -1;
    int o = 5;
    for (int k = 0; k < MM_MACROS; k++)
        for (int j = 0; j < MM_SLOTS; j++) {
            mm_slot *sl = &s->m[k].slot[j];
            int dest = in[o++];
            sl->dest = (uint8_t)(mm_dest_pos(s->dests, dest) < 0 ? 0 : dest); /* a destination this OS lacks: none */
            sl->depth = (int8_t)in[o++];
            sl->tracks = (uint16_t)(in[o] << 8 | in[o + 1]);
            o += 2;
        }
    s->dirty_mods = 1;
    return 0;
}

int mm_dest_pos(const mm_dests *d, int dest)
{
    if (!d || !dest)
        return -1;
    for (int i = 0; i < d->count; i++)
        if (d->index[i] == dest)
            return i;
    return -1;
}

void mm_dest_label(const mm_dests *d, int dest, char *buf)
{
    int p = mm_dest_pos(d, dest);
    const char *n = p >= 0 && d->name ? d->name[p] : 0;
    int i = 0;
    if (!dest) {
        n = "---";
    } else if (!n) {                /* no name known yet (spike S6): P and the index */
        buf[i++] = 'P';
        if (dest >= 100)
            buf[i++] = (char)('0' + dest / 100);
        buf[i++] = (char)('0' + dest / 10 % 10);
        buf[i++] = (char)('0' + dest % 10);
        buf[i] = 0;
        return;
    }
    for (; i < 7 && n[i]; i++)
        buf[i] = n[i];
    buf[i] = 0;
}
