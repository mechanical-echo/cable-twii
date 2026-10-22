#ifndef CTW_VOLUME_H
#define CTW_VOLUME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { VOL_USB, VOL_SD } vol_source;

typedef struct {
    vol_source source;
    bool fat;
    bool has_root;
} vol_candidate;

typedef struct {
    uint8_t type;
    uint32_t start;
    uint32_t sectors;
} vol_part_entry;

int vol_pick_root(const vol_candidate *c, size_t n);
bool vol_boot_is_fat(const uint8_t *sector);
bool vol_mbr_valid(const uint8_t *sector);
size_t vol_mbr_entries(const uint8_t *sector, vol_part_entry out[4]);
bool vol_is_extended(uint8_t type);
bool vol_ebr_parse(const uint8_t *ebr, uint32_t ext_start, uint32_t ebr_lba, vol_part_entry *logical, uint32_t *next_ebr);

#endif
