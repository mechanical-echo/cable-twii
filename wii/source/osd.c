#include "osd.h"

#include "ui/theme.h"
#include "ui/tween.h"

#define OSD_CHIP_MS THEME_CHIP_DURATION_MS
#define OSD_TIMEOUT_MS THEME_OSD_TIMEOUT_MS
#define OSD_SLIDE_MS 200u
#define OSD_VOLUME_MS THEME_VOLUME_MS
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
    osd_activity(s, now_ms);
}

bool osd_volume_visible(const osd_state *s, uint32_t now_ms)
{
    return now_ms < s->volume_until;
}

bool osd_panel_visible(const osd_state *s, uint32_t now_ms)
{
    return osd_visible(s, now_ms) && !osd_volume_visible(s, now_ms);
}

float osd_chip_glow(const osd_state *s, uint32_t now_ms)
{
    if (s->chip_until == 0 || now_ms >= s->chip_until) return 0.0f;
    uint32_t since = now_ms - (s->chip_until - OSD_CHIP_MS);
    if (since >= THEME_CHIP_GLOW_MS) return 0.0f;
    return 1.0f - (float)since / (float)THEME_CHIP_GLOW_MS;
}

float osd_chip_name_drop(const osd_state *s, uint32_t now_ms)
{
    if (s->chip_until == 0) return 0.0f;
    uint32_t since = now_ms - (s->chip_until - OSD_CHIP_MS);
    return tw_ease_out((float)since / (float)THEME_CHIP_NAME_MS);
}

int osd_volume_step(int volume, int dir)
{
    int next = dir > 0 ? (volume / VOLUME_STEP + 1) * VOLUME_STEP : ((volume + VOLUME_STEP - 1) / VOLUME_STEP - 1) * VOLUME_STEP;
    if (next > VOLUME_MAX) next = VOLUME_MAX;
    if (next < 0) next = 0;
    return next;
}
