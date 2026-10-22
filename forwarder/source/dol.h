#ifndef DOL_H
#define DOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DOL_TEXT_SECTIONS 7
#define DOL_DATA_SECTIONS 11
#define DOL_HEADER_SIZE 0x100

typedef struct {
    uint32_t offset;
    uint32_t address;
    uint32_t size;
} dol_section;

typedef struct {
    dol_section text[DOL_TEXT_SECTIONS];
    dol_section data[DOL_DATA_SECTIONS];
    uint32_t bss_address;
    uint32_t bss_size;
    uint32_t entry;
} dol_header;

bool dol_parse(const uint8_t *image, size_t size, dol_header *out);
bool dol_fits(const dol_header *h, size_t image_size, uint32_t low, uint32_t high);

#endif
