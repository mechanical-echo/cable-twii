#ifndef CTW_STORAGE_H
#define CTW_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "volume.h"

typedef struct {
    bool found;
    vol_source source;
    char name[16];
    int disk;
    uint32_t start_sector;
} st_volume;

int st_find_root(char *out, size_t out_size, st_volume *vol);

#endif
