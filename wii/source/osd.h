#ifndef CTW_OSD_H
#define CTW_OSD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t chip_until;
    uint32_t osd_until;
    uint32_t shown_at;
    uint32_t volume_until;
    bool osd;
} osd_state;

void osd_channel_changed(osd_state *s, uint32_t now_ms);
void osd_pointer(osd_state *s, bool ir_valid, bool moved, uint32_t now_ms);
bool osd_chip_visible(const osd_state *s, uint32_t now_ms);
bool osd_visible(const osd_state *s, uint32_t now_ms);
void osd_activity(osd_state *s, uint32_t now_ms);
float osd_slide(const osd_state *s, uint32_t now_ms);
void osd_volume_changed(osd_state *s, uint32_t now_ms);
bool osd_volume_visible(const osd_state *s, uint32_t now_ms);
bool osd_panel_visible(const osd_state *s, uint32_t now_ms);
float osd_chip_glow(const osd_state *s, uint32_t now_ms);
float osd_chip_name_drop(const osd_state *s, uint32_t now_ms);
int osd_volume_step(int volume, int dir);

#endif
