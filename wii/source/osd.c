#include "osd.h"

#define OSD_CHIP_MS 2000u
#define OSD_TIMEOUT_MS 3000u

void osd_channel_changed(osd_state *s, uint32_t now_ms)
{
    s->chip_until = now_ms + OSD_CHIP_MS;
}

void osd_pointer(osd_state *s, bool ir_valid, bool moved, uint32_t now_ms)
{
    if (!ir_valid) {
        s->osd = false;
        return;
    }
    if (moved) {
        s->osd = true;
        s->osd_until = now_ms + OSD_TIMEOUT_MS;
    }
}

bool osd_chip_visible(const osd_state *s, uint32_t now_ms)
{
    return now_ms < s->chip_until;
}

bool osd_visible(const osd_state *s, uint32_t now_ms)
{
    return s->osd && now_ms < s->osd_until;
}
