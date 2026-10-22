#include "guide_info.h"

#include <stdio.h>
#include <string.h>

#include "../guide_text.h"
#include "../live.h"
#include "../media/player.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define FRAME_PX 5.0f
#define PAD 14.0f
#define SHOW_ICON_PX 48.0f
#define DESC_TOP 96.0f
#define DESC_LINE_H 17.0f
#define DESC_MAX_LINES 3
#define FOOTER_H 36.0f
#define NAME_PX 18
#define SHOW_PX 15
#define BODY_PX 13
#define SMALL_PX 12

typedef struct {
    bool valid;
    int channel;
    int64_t start;
    char name[TF_LINE_BYTES];
    char show[TF_LINE_BYTES];
    char episode[TF_LINE_BYTES];
    char times[32];
    tf_line desc[DESC_MAX_LINES];
    size_t desc_count;
    const ctw_show *icon_show;
} info_text;

static info_text info;

static void draw_upnext_card(app *a, asp_rect r)
{
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, a->current_channel, a->today, a->ms_of_day, &offset);
    if (air == NULL || air->kind != CTW_AIR_UPNEXT || r.h < 80.0f) return;
    const ctw_airing *next = tn_next(&a->tuner, a->current_channel, air);
    char line[TF_LINE_BYTES], fitted[TF_LINE_BYTES], countdown[16];
    gt_line(air, next, line, sizeof line);
    ui_text_fit(FONT_BODY, 14, line, r.w - 24.0f, fitted, sizeof fitted);
    uint32_t left_s = tn_ms_remaining(air, offset) / 1000;
    snprintf(countdown, sizeof countdown, "%u:%02u", (unsigned)(left_s / 60), (unsigned)(left_s % 60));
    float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f;
    ui_text(FONT_BODY, 14, cx, cy - 22.0f, fitted, THEME_PREVIEW_TEXT, ALIGN_CENTER);
    ui_text(FONT_BOLD, 20, cx, cy + 2.0f, countdown, THEME_ACCENT, ALIGN_CENTER);
}

void ginfo_draw_preview(app *a, asp_rect r, float expand)
{
    float keep = 1.0f - expand;
    float border = FRAME_PX * keep;
    float radius = THEME_PANEL_RADIUS * keep;
    if (keep > 0.0f) ui_panel(r.x, r.y, r.w, r.h, radius, THEME_PANEL_FILL, THEME_PANEL_BORDER);
    asp_rect in = { r.x + border * ui_pixel_x(), r.y + border, r.w - 2.0f * border * ui_pixel_x(), r.h - 2.0f * border };
    float in_radius = radius > border ? radius - border : 0.0f;
    ui_rounded(in.x, in.y, in.w, in.h, in_radius, THEME_PREVIEW_FILL);
    if (live_on(a->current_channel)) {
        if (pl_status() != PL_IDLE) {
            pl_draw(in.x, in.y, in.w, in.h, live_fade_in(a, THEME_PREVIEW_FADE_MS));
        } else {
            draw_upnext_card(a, in);
        }
    }
    if (keep > 0.0f) ui_corner_mask(in.x, in.y, in.w, in.h, in_radius, THEME_PANEL_FILL);
}

static void refresh(app *a, const ctw_channel *ch, const ctw_airing *air, int64_t start, asp_rect r)
{
    memset(&info, 0, sizeof info);
    info.valid = true;
    info.channel = ch->number;
    info.start = start;
    float text_w = r.w - 2.0f * PAD;
    float side_w = text_w - SHOW_ICON_PX - 10.0f;
    ui_text_fit(FONT_BOLD, NAME_PX, ch->name, text_w, info.name, sizeof info.name);
    if (air == NULL) return;

    const ctw_airing *next = air->kind == CTW_AIR_UPNEXT ? tn_next(&a->tuner, ch->number, air) : NULL;
    const ctw_airing *about = air->kind == CTW_AIR_UPNEXT ? next : air;
    info.icon_show = gt_line_show(air, next);

    char buf[TF_LINE_BYTES];
    if (air->kind == CTW_AIR_EPISODE && air->show != NULL) snprintf(buf, sizeof buf, "%s", air->show->name);
    else gt_line(air, next, buf, sizeof buf);
    ui_text_fit(FONT_BOLD, SHOW_PX, buf, side_w, info.show, sizeof info.show);

    if (about != NULL && about->kind == CTW_AIR_EPISODE && about->episode != NULL) {
        gt_episode(about->episode, buf, sizeof buf);
        ui_text_fit(FONT_BODY, BODY_PX, buf, side_w, info.episode, sizeof info.episode);
        float room = r.h - FOOTER_H - DESC_TOP;
        size_t lines = room > 0.0f ? (size_t)(room / DESC_LINE_H) : 0;
        if (lines > DESC_MAX_LINES) lines = DESC_MAX_LINES;
        const char *desc = about->episode->description;
        if (lines > 0 && desc != NULL) info.desc_count = ui_text_wrap(FONT_BODY, BODY_PX, desc, text_w, lines, info.desc);
    }
    gt_times(air->start_ms, air->duration_ms, info.times, sizeof info.times);
}

void ginfo_draw_info(app *a, const ctw_channel *ch, asp_rect r)
{
    ui_panel(r.x, r.y, r.w, r.h, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);
    if (ch == NULL) return;
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, ch->number, a->today, a->ms_of_day, &offset);
    int64_t start = air != NULL ? tn_airing_start(&a->tuner, ch->number, air) : -1;
    if (!info.valid || info.channel != ch->number || info.start != start) refresh(a, ch, air, start, r);

    float x = r.x + PAD;
    ui_text(FONT_BOLD, NAME_PX, x, r.y + 12.0f, info.name, THEME_ACCENT, ALIGN_LEFT);
    if (air == NULL) return;

    char path[320];
    gt_icon_path(a->root, ch, info.icon_show, path, sizeof path);
    icons_draw(path, x, r.y + 40.0f, SHOW_ICON_PX);
    float sx = x + SHOW_ICON_PX + 10.0f;
    ui_text(FONT_BOLD, SHOW_PX, sx, r.y + 44.0f, info.show, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, BODY_PX, sx, r.y + 66.0f, info.episode, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    for (size_t i = 0; i < info.desc_count; i++) {
        ui_text(FONT_BODY, BODY_PX, x, r.y + DESC_TOP + DESC_LINE_H * (float)i, info.desc[i].text, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    }
    ui_text(FONT_BODY, SMALL_PX, x, r.y + r.h - 32.0f, info.times, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    ui_bar(x, r.y + r.h - 14.0f, r.w - 2.0f * PAD, 5.0f, gt_progress(air, offset));
}
