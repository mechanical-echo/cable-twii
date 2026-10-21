#include "upnext.h"

#include <stdio.h>

#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/theme.h"

static void format_hhmm(int64_t start_ms, char *out, size_t out_size)
{
    int64_t ms_of_day = start_ms % (24 * 3600000LL);
    if (ms_of_day < 0) ms_of_day += 24 * 3600000LL;
    int hh = (int)(ms_of_day / 3600000);
    int mm = (int)((ms_of_day / 60000) % 60);
    snprintf(out, out_size, "at %02d:%02d", hh, mm);
}

static void format_countdown(uint32_t ms_left, char *out, size_t out_size)
{
    uint32_t total_s = ms_left / 1000;
    uint32_t mm = total_s / 60;
    uint32_t ss = total_s % 60;
    snprintf(out, out_size, "%u:%02u", mm, ss);
}

void upnext_draw(app *a, const ctw_airing *gap, const ctw_airing *next, uint32_t ms_left)
{
    (void)gap;
    ui_stripes();

    const float px = 170.0f, py = 140.0f, pw = 300.0f, ph = 200.0f;
    ui_panel(px, py, pw, ph, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);

    ui_text(FONT_BOLD, 16, px + pw / 2.0f, py + 20.0f, "Up next", THEME_ACCENT, ALIGN_CENTER);

    char title[128] = "";
    if (next != NULL) {
        if (next->kind == CTW_AIR_AD) {
            snprintf(title, sizeof title, "Commercial break");
        } else if (next->kind == CTW_AIR_EPISODE && next->show != NULL && next->episode != NULL) {
            snprintf(title, sizeof title, "%s", next->show->name);
        }
    }
    ui_text(FONT_BODY, 15, px + pw / 2.0f, py + 56.0f, title, THEME_TEXT_PRIMARY, ALIGN_CENTER);

    if (next != NULL && next->kind == CTW_AIR_EPISODE && next->episode != NULL) {
        ui_text(FONT_BODY, 13, px + pw / 2.0f, py + 80.0f, next->episode->title, THEME_TEXT_SECONDARY, ALIGN_CENTER);
    }

    if (next != NULL) {
        char at[32];
        format_hhmm(next->start_ms, at, sizeof at);
        ui_text(FONT_BODY, 13, px + pw / 2.0f, py + 112.0f, at, THEME_TEXT_SECONDARY, ALIGN_CENTER);
    }

    char countdown[16];
    format_countdown(ms_left, countdown, sizeof countdown);
    ui_text(FONT_BOLD, 28, px + pw / 2.0f, py + 140.0f, countdown, THEME_ACCENT, ALIGN_CENTER);

    (void)a;
}
