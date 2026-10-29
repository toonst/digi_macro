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

/* The slots, then the names (zero-padded): the part the blob and the project's setups share. */
static int body_encode(const mm_t *s, uint8_t *out)
{
    int o = 0;
    for (int k = 0; k < MM_MACROS; k++)
        for (int j = 0; j < MM_SLOTS; j++) {
            const mm_slot *sl = &s->m[k].slot[j];
            out[o++] = sl->dest;
            out[o++] = (uint8_t)((uint16_t)sl->depth >> 8);
            out[o++] = (uint8_t)sl->depth;
            out[o++] = (uint8_t)(sl->tracks >> 8);
            out[o++] = (uint8_t)sl->tracks;
        }
    for (int k = 0; k < MM_MACROS; k++)
        for (int i = 0; i < MM_NAME_LEN; i++)
            out[o++] = (uint8_t)s->m[k].name[i];
    return o;
}

/* v is the blob's version: 1 has a depth byte and no names. */
static void body_decode(mm_t *s, const uint8_t *in, int v)
{
    int o = 0;
    for (int k = 0; k < MM_MACROS; k++)
        for (int j = 0; j < MM_SLOTS; j++) {
            mm_slot *sl = &s->m[k].slot[j];
            int dest = in[o++];
            sl->dest = (uint8_t)(mm_dest_pos(s->dests, dest) < 0 ? 0 : dest); /* a destination this OS lacks: none */
            if (v == 1) {
                int d = (int8_t)in[o++] * 2 * MM_DEPTH_ONE;  /* -64 .. +63 was the full range */
                sl->depth = (int16_t)(d > MM_DEPTH_MAX ? MM_DEPTH_MAX : d);
            } else {
                sl->depth = (int16_t)(in[o] << 8 | in[o + 1]);
                o += 2;
            }
            sl->tracks = (uint16_t)(in[o] << 8 | in[o + 1]);
            if (!sl->dest && !sl->tracks)   /* an unused slot of an older record: every track, as a fresh one */
                sl->tracks = (uint16_t)((1u << MM_TRACKS) - 1);
            o += 2;
        }
    for (int k = 0; k < MM_MACROS; k++) {
        for (int i = 0; i < MM_NAME_LEN; i++)
            s->m[k].name[i] = v >= 2 ? (char)in[o++] : 0;
        s->m[k].name[MM_NAME_LEN] = 0;
    }
    s->dirty_mods = s->dirty_screen = 1;
}

int mm_encode(const mm_t *s, uint8_t *out)
{
    int o = 0;
    out[o++] = 'M';
    out[o++] = 'M';
    out[o++] = MM_BLOB_VERSION;
    out[o++] = MM_MACROS;
    out[o++] = MM_SLOTS;
    o += body_encode(s, out + o);
    uint16_t c = crc16(out, o);
    out[o++] = (uint8_t)(c >> 8);
    out[o++] = (uint8_t)c;
    return o;
}

/* Versions 1 (depth -64 .. +63 in a byte, no names) and 2. len is what the buffer holds; the version gives the size. */
int mm_decode(mm_t *s, const uint8_t *in, int len)
{
    int v = len >= 5 ? in[2] : 0, size = v == 1 ? MM_BLOB_V1_SIZE : v == 2 ? MM_BLOB_SIZE : 0;
    if (!size || len < size || in[0] != 'M' || in[1] != 'M' || in[3] != MM_MACROS || in[4] != MM_SLOTS)
        return -1;
    if (crc16(in, size - 2) != (uint16_t)(in[size - 2] << 8 | in[size - 1]))
        return -1;
    body_decode(s, in + 5, v);
    return 0;
}

/* A setup in the project (docs/findings/s7-project-storage.md): "MS", flags, the slots and names, a CRC-16. */
void mm_setup_pack(const mm_t *s, int flags, uint8_t *out)
{
    int o = 0;
    out[o++] = 'M';
    out[o++] = 'S';
    out[o++] = (uint8_t)flags;
    o += body_encode(s, out + o);
    uint16_t c = crc16(out, o);
    out[o++] = (uint8_t)(c >> 8);
    out[o] = (uint8_t)c;
}

int mm_setup_valid(const uint8_t *in)
{
    return in[0] == 'M' && in[1] == 'S' &&
           crc16(in, MM_SETUP_SIZE - 2) == (uint16_t)(in[MM_SETUP_SIZE - 2] << 8 | in[MM_SETUP_SIZE - 1]);
}

int mm_setup_unpack(mm_t *s, const uint8_t *in)
{
    if (!mm_setup_valid(in))
        return -1;
    body_decode(s, in + 3, MM_BLOB_VERSION);  /* the knobs' positions stay */
    return in[2];
}

void mm_set_name(mm_t *s, int macro, const char *name)
{
    char *n = s->m[macro].name;
    int i = 0;
    for (; i < MM_NAME_LEN && name && name[i]; i++)
        n[i] = name[i];
    while (i > 0 && n[i - 1] == ' ')
        i--;
    for (; i <= MM_NAME_LEN; i++)
        n[i] = 0;
    s->dirty_screen = s->dirty_config = 1;
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

void mm_record_encode(const mm_t *s, uint32_t seq, uint8_t *out)
{
    for (int i = 0; i < MM_RECORD_SIZE; i++)
        out[i] = 0;
    out[0] = 'M', out[1] = 'M', out[2] = 'C', out[3] = 'F';
    out[4] = (uint8_t)(seq >> 24), out[5] = (uint8_t)(seq >> 16), out[6] = (uint8_t)(seq >> 8), out[7] = (uint8_t)seq;
    mm_encode(s, out + 8);
}

static int record_valid(const mm_t *s, const uint8_t *r)
{
    mm_t probe;
    uint8_t *p = (uint8_t *)&probe;
    for (unsigned i = 0; i < sizeof probe; i++)
        p[i] = 0;
    probe.dests = s->dests;
    return r[0] == 'M' && r[1] == 'M' && r[2] == 'C' && r[3] == 'F' && mm_decode(&probe, r + 8, MM_BLOB_SIZE) == 0;
}

int mm_record_pick(const mm_t *s, const uint8_t *a, const uint8_t *b, uint32_t *seq)
{
    int va = record_valid(s, a), vb = record_valid(s, b);
    int pick = va && vb ? (be32(b + 4) > be32(a + 4) ? 1 : 0) : va ? 0 : vb ? 1 : -1;
    if (pick >= 0)
        *seq = be32((pick ? b : a) + 4);
    return pick;
}

int mm_dest_pos(const mm_dests *d, int dest)
{
    if (!d || !dest)
        return -1;
    for (int i = 0; i < d->count; i++)
        if (d->d[i].index == dest)
            return i;
    return -1;
}

void mm_dest_label(const mm_dests *d, int dest, char *buf)
{
    int p = mm_dest_pos(d, dest);
    const char *n = p >= 0 ? d->d[p].name : 0;
    int i = 0;
    if (!dest) {
        n = "---";
    } else if (!n) {                /* no name known: P and the index */
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

const char *mm_dest_group(const mm_dests *d, int dest)
{
    /* the stock modulation setup heads an unset destination META */
    static const char *const groups[MM_G_COUNT] = {"META", "SRC", "FLTR", "AMP", "FX", "PORT"};
    int p = mm_dest_pos(d, dest);
    int g = p >= 0 ? d->d[p].group : MM_G_NONE;
    return groups[g < MM_G_COUNT ? g : MM_G_NONE];
}

/* The destination list (the setup screen's popup, as the stock modulation setups have): line 0 is none, then each
 * page's destinations after a separator line. */
static int new_group(const mm_dests *d, int i)
{
    return i == 0 || d->d[i].group != d->d[i - 1].group;
}

int mm_dest_lines(const mm_dests *d)
{
    int n = 1;
    for (int i = 0; d && i < d->count; i++)
        n += new_group(d, i) + 1;
    return n;
}

int mm_dest_line(const mm_dests *d, int dest)
{
    int line = 1;
    if (!d || !dest)
        return 0;
    for (int i = 0; i < d->count; i++) {
        line += new_group(d, i);
        if (d->d[i].index == dest)
            return line;
        line++;
    }
    return 0;
}

int mm_line_dest(const mm_dests *d, int line, int *group_first)
{
    int l = 1;
    *group_first = 1;
    if (line == 0)
        return 0;
    for (int i = 0; d && i < d->count; i++) {
        if (new_group(d, i) && l++ == line)
            return -1;
        if (l++ == line) {
            *group_first = new_group(d, i);
            return d->d[i].index;
        }
    }
    return -1;
}
