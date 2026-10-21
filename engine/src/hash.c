#include "ctw/ctw.h"

#define CTW_FNV_PRIME 0x100000001b3ULL

uint64_t ctw_fnv1a(uint64_t h, const void *data, size_t len)
{
    const unsigned char *p = data;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= CTW_FNV_PRIME;
    }
    return h;
}

static uint64_t hash_u32_be(uint64_t h, uint32_t v)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v >> 24);
    b[1] = (unsigned char)(v >> 16);
    b[2] = (unsigned char)(v >> 8);
    b[3] = (unsigned char)v;
    return ctw_fnv1a(h, b, 4);
}

uint64_t ctw_day_seed(uint32_t seed, int channel_number, ctw_date day)
{
    uint64_t h = CTW_FNV_OFFSET;
    h = hash_u32_be(h, seed);
    h = hash_u32_be(h, (uint32_t)channel_number);
    h = hash_u32_be(h, ctw_date_key(day));
    return h;
}
