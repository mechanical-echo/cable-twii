#ifndef CTW_OSD_H
#define CTW_OSD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t chip_until;
    uint32_t osd_until;
    bool osd;
} osd_state;

void osd_channel_changed(osd_state *s, uint32_t now_ms);
void osd_pointer(osd_state *s, bool ir_valid, bool moved, uint32_t now_ms);
bool osd_chip_visible(const osd_state *s, uint32_t now_ms);
bool osd_visible(const osd_state *s, uint32_t now_ms);

#endif
