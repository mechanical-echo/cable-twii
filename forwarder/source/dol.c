#include "dol.h"

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

bool dol_parse(const uint8_t *image, size_t size, dol_header *out)
{
    if (size < DOL_HEADER_SIZE) return false;
    for (int i = 0; i < DOL_TEXT_SECTIONS; i++) {
        out->text[i].offset = be32(image + 0x00 + 4 * i);
        out->text[i].address = be32(image + 0x48 + 4 * i);
        out->text[i].size = be32(image + 0x90 + 4 * i);
    }
    for (int i = 0; i < DOL_DATA_SECTIONS; i++) {
        out->data[i].offset = be32(image + 0x1C + 4 * i);
        out->data[i].address = be32(image + 0x64 + 4 * i);
        out->data[i].size = be32(image + 0xAC + 4 * i);
    }
    out->bss_address = be32(image + 0xD8);
    out->bss_size = be32(image + 0xDC);
    out->entry = be32(image + 0xE0);
    return true;
}

static bool range_in(uint32_t start, uint32_t size, uint32_t low, uint32_t high)
{
    if (size == 0) return true;
    uint32_t end = start + size;
    return end > start && start >= low && end <= high;
}

static bool section_fits(const dol_section *s, size_t image_size, uint32_t low, uint32_t high)
{
    if (s->size == 0) return true;
    uint64_t file_end = (uint64_t)s->offset + s->size;
    return file_end <= image_size && range_in(s->address, s->size, low, high);
}

bool dol_fits(const dol_header *h, size_t image_size, uint32_t low, uint32_t high)
{
    for (int i = 0; i < DOL_TEXT_SECTIONS; i++) {
        if (!section_fits(&h->text[i], image_size, low, high)) return false;
    }
    for (int i = 0; i < DOL_DATA_SECTIONS; i++) {
        if (!section_fits(&h->data[i], image_size, low, high)) return false;
    }
    return range_in(h->bss_address, h->bss_size, low, high) && range_in(h->entry, 4, low, high);
}
