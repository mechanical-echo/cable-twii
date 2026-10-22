#include "upnext.h"

#include <stdio.h>
#include <string.h>

#include "../guide_text.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define ICON_PX 40.0f
#define TEXT_PAD 16.0f
#define TITLE_PX 15
#define EPISODE_PX 13

typedef struct {
    bool valid;
    int channel;
    const ctw_airing *next;
    int64_t start;
    char title[TF_LINE_BYTES];
    char episode[TF_LINE_BYTES];
    char at[32];
} upnext_text;

static upnext_text text;

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

static void refresh(const ctw_airing *next, float max_w)
{
    memset(&text, 0, sizeof text);
    text.valid = true;
    text.next = next;
    text.start = next != NULL ? next->start_ms : 0;
    if (next == NULL) return;
    char title[128] = "";
    if (next->kind == CTW_AIR_AD) {
        snprintf(title, sizeof title, "Commercial break");
    } else if (next->kind == CTW_AIR_EPISODE && next->show != NULL && next->episode != NULL) {
        snprintf(title, sizeof title, "%s", next->show->name);
    }
    ui_text_fit(FONT_BODY, TITLE_PX, title, max_w, text.title, sizeof text.title);
    if (next->kind == CTW_AIR_EPISODE && next->episode != NULL) {
        ui_text_fit(FONT_BODY, EPISODE_PX, next->episode->title, max_w, text.episode, sizeof text.episode);
    }
    format_hhmm(next->start_ms, text.at, sizeof text.at);
}

void upnext_draw(app *a, const ctw_airing *gap, const ctw_airing *next, uint32_t ms_left)
{
    (void)gap;
    ui_stripes();

    const float px = 170.0f, py = 124.0f, pw = 300.0f, ph = 232.0f;
    const float cx = px + pw / 2.0f;
    ui_panel(px, py, pw, ph, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);

    const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
    char path[320] = "";
    if (ch != NULL) gt_icon_path(a->root, ch, NULL, path, sizeof path);
    icons_draw(path, cx - ICON_PX / 2.0f, py + 16.0f, ICON_PX);

    ui_text(FONT_BOLD, 16, cx, py + 64.0f, "Up next", THEME_ACCENT, ALIGN_CENTER);

    if (!text.valid || text.next != next || (next != NULL && text.start != next->start_ms) || text.channel != a->current_channel) {
        refresh(next, pw - 2.0f * TEXT_PAD);
        text.channel = a->current_channel;
    }
    ui_text(FONT_BODY, TITLE_PX, cx, py + 94.0f, text.title, THEME_TEXT_PRIMARY, ALIGN_CENTER);
    ui_text(FONT_BODY, EPISODE_PX, cx, py + 118.0f, text.episode, THEME_TEXT_SECONDARY, ALIGN_CENTER);
    ui_text(FONT_BODY, 13, cx, py + 146.0f, text.at, THEME_TEXT_SECONDARY, ALIGN_CENTER);

    char countdown[16];
    format_countdown(ms_left, countdown, sizeof countdown);
    ui_text(FONT_BOLD, 28, cx, py + 172.0f, countdown, THEME_ACCENT, ALIGN_CENTER);
}
