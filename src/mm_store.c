/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macro-mode: the stored configuration (PLAN.md 4.7) and the destination list (W4). */
#include "mm.h"

/* "MM", version, macros, slots; then per slot dest, depth, tracks (big-endian); then a CRC-16/CCITT of all before. */
static uint16_t crc16_from(uint16_t c, const uint8_t *p, int n)
{
    while (n--) {
        c ^= (uint16_t)(*p++ << 8);
        for (int b = 0; b < 8; b++)
            c = (uint16_t)(c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1);
    }
    return c;
}

static uint16_t crc16(const uint8_t *p, int n)
{
    return crc16_from(0xffff, p, n);
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

/* A setup with a kit or the project (docs/findings/s7-project-storage.md, mm.h's MM_SETUP_SIZE): a bit stream, high
 * bit first: the destinations three by three in base MM_DEST_RADIX, per slot the depth (16 bits) and tracks (16),
 * the names in base 60, the positions + 64 (7 bits), a 1. elekloader's core keeps it apart from other mods' data and other
 * tools' bytes, so it needs no mark or check of its own. */
static void put_bits(uint8_t *p, int *bit, uint32_t v, int n)
{
    while (n--) {
        if (v >> n & 1)
            p[*bit >> 3] |= (uint8_t)(0x80 >> (*bit & 7));
        (*bit)++;
    }
}

static uint32_t get_bits(const uint8_t *p, int *bit, int n)
{
    uint32_t v = 0;
    while (n--) {
        v = v << 1 | (p[*bit >> 3] >> (7 - (*bit & 7)) & 1);
        (*bit)++;
    }
    return v;
}

/* The naming screen's characters, after the space (code 0): DT2 1.17's NameView set (the string at 0x4025dea9), its
 * Latin-1 letters included. A character outside it is kept as a space. */
static const char name_chars[MM_NAME_RADIX] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ\xc5\xc4\xd6\xdc\xdf\xc6\xd8\xc7\xd1" "0123456789~!@#$%^&()_+-=";

static uint32_t name_code(char c)
{
    for (int i = 0; i < MM_NAME_RADIX - 1; i++)
        if (name_chars[i] == c)
            return (uint32_t)i + 1;
    return 0;
}

static const mm_slot *slot_at(const mm_t *s, int i)
{
    return &s->m[i / MM_SLOTS].slot[i % MM_SLOTS];
}

static uint32_t dest_code(const mm_t *s, int i)
{
    int d = slot_at(s, i)->dest;
    return d > 0 && d < MM_DEST_RADIX ? (uint32_t)d : 0;
}

void mm_setup_pack(const mm_t *s, uint8_t *out)
{
    int bit = 0, i;
    for (i = 0; i < MM_SETUP_SIZE; i++)
        out[i] = 0;
    for (i = 0; i + 2 < MM_MACROS * MM_SLOTS; i += 3)
        put_bits(out, &bit, dest_code(s, i) + MM_DEST_RADIX * (dest_code(s, i + 1) + MM_DEST_RADIX * dest_code(s, i + 2)),
                 MM_DEST3_BITS);
    put_bits(out, &bit, dest_code(s, i) + MM_DEST_RADIX * dest_code(s, i + 1), MM_DEST2_BITS);
    for (i = 0; i < MM_MACROS * MM_SLOTS; i++) {
        put_bits(out, &bit, (uint16_t)slot_at(s, i)->depth, 16);
        put_bits(out, &bit, slot_at(s, i)->tracks, 16);
    }
    for (int k = 0; k < MM_MACROS; k++) {
        uint32_t v = 0;
        int n = 0;
        while (n < MM_NAME_LEN && s->m[k].name[n])
            n++;
        while (n--)                 /* the first character is the lowest digit */
            v = v * MM_NAME_RADIX + name_code(s->m[k].name[n]);
        put_bits(out, &bit, v, MM_NAME_BITS);
    }
    for (int k = 0; k < MM_MACROS; k++)
        put_bits(out, &bit, (uint32_t)(s->m[k].pos - MM_POS_MIN), 7);
    put_bits(out, &bit, 1, 1);      /* a setup is never all zero */
}

int mm_setup_valid(const uint8_t *in)
{
    for (int i = 0; i < MM_SETUP_SIZE; i++)
        if (in[i])
            return 1;
    return 0;
}

static void set_dest(mm_t *s, int i, uint32_t d)
{
    mm_slot *sl = &s->m[i / MM_SLOTS].slot[i % MM_SLOTS];
    sl->dest = (uint8_t)(d < MM_DEST_RADIX && mm_dest_pos(s->dests, (int)d) >= 0 ? d : 0);
}

int mm_setup_unpack(mm_t *s, const uint8_t *in)
{
    int bit = 0, i;
    uint32_t v;
    if (!mm_setup_valid(in))
        return -1;
    for (i = 0; i + 2 < MM_MACROS * MM_SLOTS; i += 3) {
        v = get_bits(in, &bit, MM_DEST3_BITS);
        set_dest(s, i, v % MM_DEST_RADIX);
        set_dest(s, i + 1, v / MM_DEST_RADIX % MM_DEST_RADIX);
        set_dest(s, i + 2, v / (MM_DEST_RADIX * MM_DEST_RADIX));
    }
    v = get_bits(in, &bit, MM_DEST2_BITS);
    set_dest(s, i, v % MM_DEST_RADIX);
    set_dest(s, i + 1, v / MM_DEST_RADIX);
    for (i = 0; i < MM_MACROS * MM_SLOTS; i++) {
        mm_slot *sl = &s->m[i / MM_SLOTS].slot[i % MM_SLOTS];
        sl->depth = (int16_t)get_bits(in, &bit, 16);
        sl->tracks = (uint16_t)get_bits(in, &bit, 16);
    }
    for (int k = 0; k < MM_MACROS; k++) {
        char *n = s->m[k].name;
        int len = 0;
        v = get_bits(in, &bit, MM_NAME_BITS);
        for (int c = 0; c < MM_NAME_LEN; c++, v /= MM_NAME_RADIX) {
            uint32_t code = v % MM_NAME_RADIX;
            n[c] = code ? name_chars[code - 1] : ' ';
            if (code)
                len = c + 1;
        }
        for (int c = len; c <= MM_NAME_LEN; c++) /* trailing spaces dropped, as the naming does */
            n[c] = 0;
    }
    for (int k = 0; k < MM_MACROS; k++)
        s->m[k].pos = (int8_t)((int)get_bits(in, &bit, 7) + MM_POS_MIN);
    s->dirty_mods = s->dirty_screen = 1;
    return 0;
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
