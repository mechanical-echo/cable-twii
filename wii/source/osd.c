#include "osd.h"

#include "ui/tween.h"

#define OSD_CHIP_MS 2000u
#define OSD_TIMEOUT_MS 3000u
#define OSD_SLIDE_MS 200u
#define OSD_VOLUME_MS 2000u
#define VOLUME_STEP 10
#define VOLUME_MAX 100

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
        if (!osd_visible(s, now_ms)) s->shown_at = now_ms;
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

void osd_activity(osd_state *s, uint32_t now_ms)
{
    if (!osd_visible(s, now_ms)) return;
    s->osd_until = now_ms + OSD_TIMEOUT_MS;
}

float osd_slide(const osd_state *s, uint32_t now_ms)
{
    return tw_ease_out((float)(now_ms - s->shown_at) / (float)OSD_SLIDE_MS);
}

void osd_volume_changed(osd_state *s, uint32_t now_ms)
{
    s->volume_until = now_ms + OSD_VOLUME_MS;
}

bool osd_volume_alone(const osd_state *s, uint32_t now_ms)
{
    return now_ms < s->volume_until && !osd_visible(s, now_ms);
}

int osd_volume_step(int volume, int dir)
{
    int next = dir > 0 ? (volume / VOLUME_STEP + 1) * VOLUME_STEP : ((volume + VOLUME_STEP - 1) / VOLUME_STEP - 1) * VOLUME_STEP;
    if (next > VOLUME_MAX) next = VOLUME_MAX;
    if (next < 0) next = 0;
    return next;
}
