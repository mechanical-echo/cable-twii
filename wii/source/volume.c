#include "volume.h"

#include <string.h>

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static bool is_pow2(uint32_t v)
{
    return v != 0 && (v & (v - 1)) == 0;
}

int vol_pick_root(const vol_candidate *c, size_t n)
{
    int sd = -1;
    for (size_t i = 0; i < n; i++) {
        if (!c[i].fat || !c[i].has_root) continue;
        if (c[i].source == VOL_USB) return (int)i;
        if (sd < 0) sd = (int)i;
    }
    return sd;
}

bool vol_boot_is_fat(const uint8_t *s)
{
    if (s[510] != 0x55 || s[511] != 0xAA) return false;
    if (memcmp(s, "WBFS", 4) == 0) return false;
    if (memcmp(s + 3, "NTFS    ", 8) == 0 || memcmp(s + 3, "EXFAT   ", 8) == 0) return false;
    uint16_t bps = le16(s + 11);
    if (bps < 512 || bps > 4096 || !is_pow2(bps)) return false;
    if (!is_pow2(s[13])) return false;
    if (le16(s + 14) == 0) return false;
    if (s[16] == 0) return false;
    return memcmp(s + 0x52, "FAT", 3) == 0 || memcmp(s + 0x36, "FAT", 3) == 0;
}

bool vol_mbr_valid(const uint8_t *s)
{
    return s[510] == 0x55 && s[511] == 0xAA;
}

size_t vol_mbr_entries(const uint8_t *s, vol_part_entry out[4])
{
    size_t n = 0;
    for (int i = 0; i < 4; i++) {
        const uint8_t *e = s + 446 + i * 16;
        uint32_t start = le32(e + 8);
        uint32_t count = le32(e + 12);
        if (e[4] == 0 || count == 0) continue;
        out[n].type = e[4];
        out[n].start = start;
        out[n].sectors = count;
        n++;
    }
    return n;
}

bool vol_is_extended(uint8_t type)
{
    return type == 0x05 || type == 0x0F || type == 0x85;
}

bool vol_ebr_parse(const uint8_t *ebr, uint32_t ext_start, uint32_t ebr_lba, vol_part_entry *logical, uint32_t *next_ebr)
{
    if (!vol_mbr_valid(ebr)) return false;
    const uint8_t *e0 = ebr + 446;
    const uint8_t *e1 = ebr + 446 + 16;
    logical->type = e0[4];
    logical->start = ebr_lba + le32(e0 + 8);
    logical->sectors = le32(e0 + 12);
    *next_ebr = vol_is_extended(e1[4]) && le32(e1 + 12) != 0 ? ext_start + le32(e1 + 8) : 0;
    return true;
}
