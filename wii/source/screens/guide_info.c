#include "guide_info.h"

#include <stdio.h>
#include <string.h>
#include <grrlib.h>

#include "../guide_layout.h"
#include "../guide_text.h"
#include "../live.h"
#include "../media/player.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define FRAME_PX 5.0f
#define ICON_PX 48.0f
#define ICON_X 11.3f
#define NAME_PX 18
#define NAME_Y 22.2f
#define NAME_GAP 8.0f
#define LINE_PX 13
#define LINE_Y 46.2f
#define TIMES_PX 12
#define TIMES_Y 66.0f
#define BAR_Y 69.6f
#define BAR_H 6.2f
#define BAR_GAP 4.6f
#define BAR_RIGHT 212.0f
#define DIVIDER_X 220.0f
#define DIVIDER_Y 17.6f
#define DIVIDER_H 66.7f
#define DIVIDER_GAP 6.0f
#define NEXT_X 228.7f
#define NEXT_W 73.3f
#define NEXT_LABEL_PX 12
#define NEXT_LABEL_Y 15.1f
#define NEXT_TITLE_PX 12
#define NEXT_TITLE_Y 31.6f
#define NEXT_LINE_H 14.4f
#define NEXT_LINES 2
#define COUNTDOWN_PX 21
#define COUNTDOWN_Y 63.2f
#define HINT_X 9.0f
#define HINT_BOTTOM 33.8f
#define HINT_W 73.3f
#define HINT_H 26.7f
#define HINT_DISC_X 2.7f
#define HINT_DISC_Y 3.6f
#define HINT_DISC_H 19.6f
#define HINT_LABEL_GAP 4.0f
#define HINT_LABEL_Y 6.4f
#define HINT_PX 12
#define GAP_TEXT_PX 14
#define GAP_COUNT_PX 20

typedef struct {
    bool valid;
    int channel;
    int64_t start;
    const ctw_airing *air;
    char name[TF_LINE_BYTES];
    char line[TF_LINE_BYTES];
    char times[32];
    float times_w;
    tf_line next[NEXT_LINES];
    size_t next_count;
    const ctw_show *icon_show;
    const ctw_airing *next_ep;
} info_text;

static const ui_stop HINT_DISC_STOPS[] = { { 0.0f, THEME_PANEL_FILL }, { 1.0f, THEME_HINT_DISC_BOTTOM } };
static info_text info;

static void draw_upnext_card(app *a, asp_rect r)
{
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, a->current_channel, a->today, a->ms_of_day, &offset);
    if (air == NULL || air->kind != CTW_AIR_UPNEXT || r.h < 80.0f) return;
    const ctw_airing *next = tn_next(&a->tuner, a->current_channel, air);
    char line[TF_LINE_BYTES], fitted[TF_LINE_BYTES], countdown[16];
    gt_line(air, next, line, sizeof line);
    ui_text_fit(FONT_BODY, GAP_TEXT_PX, line, r.w - 24.0f, fitted, sizeof fitted);
    gt_countdown(tn_ms_remaining(air, offset), countdown, sizeof countdown);
    float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f;
    ui_text(FONT_BODY, GAP_TEXT_PX, cx, cy - 22.0f, fitted, THEME_PREVIEW_TEXT, ALIGN_CENTER);
    ui_text(FONT_BOLD, GAP_COUNT_PX, cx, cy + 2.0f, countdown, THEME_ACCENT, ALIGN_CENTER);
}

static void draw_hint(asp_rect in)
{
    float hx = in.x + HINT_X, hy = in.y + in.h - HINT_BOTTOM;
    ui_rounded(hx, hy, HINT_W, HINT_H, HINT_H / 2.0f, THEME_HINT_FILL);
    float dw = HINT_DISC_H * ui_pixel_x();
    float dx = hx + HINT_DISC_X, dy = hy + HINT_DISC_Y;
    ui_rounded_gradient(dx, dy, dw, HINT_DISC_H, HINT_DISC_H / 2.0f, HINT_DISC_STOPS, 2);
    ui_text(FONT_BOLD, HINT_PX, dx + dw / 2.0f, dy + (HINT_DISC_H - (float)HINT_PX) / 2.0f, "A", THEME_TEXT_PRIMARY, ALIGN_CENTER);
    ui_text(FONT_BOLD, HINT_PX, dx + dw + HINT_LABEL_GAP, hy + HINT_LABEL_Y, "Watch", THEME_PANEL_FILL, ALIGN_LEFT);
}

void ginfo_draw_preview(app *a, asp_rect r, float expand)
{
    float keep = 1.0f - expand;
    float border = FRAME_PX * keep;
    float radius = THEME_PANEL_RADIUS * keep;
    if (keep > 0.0f) ui_card(r.x, r.y, r.w, r.h, radius, 0.0f, THEME_GLOSS_BOTTOM, keep >= 1.0f ? UI_SHADOW_PANEL : UI_SHADOW_NONE);
    asp_rect in = { r.x + border * ui_pixel_x(), r.y + border, r.w - 2.0f * border * ui_pixel_x(), r.h - 2.0f * border };
    float in_radius = radius > border ? radius - border : 0.0f;
    ui_rounded(in.x, in.y, in.w, in.h, in_radius, THEME_PREVIEW_FILL);
    bool live = live_on(a->current_channel);
    if (live) {
        if (pl_status() != PL_IDLE) {
            pl_draw(in.x, in.y, in.w, in.h, live_fade_in(a, THEME_PREVIEW_FADE_MS));
        } else {
            draw_upnext_card(a, in);
        }
    }
    if (keep > 0.0f) ui_corner_mask(in.x, in.y, in.w, in.h, in_radius, THEME_PANEL_FILL);
    if (keep >= 1.0f && live) draw_hint(in);
}

static float text_x(asp_rect r)
{
    return r.x + ICON_X + ICON_PX * ui_pixel_x() + NAME_GAP;
}

static void refresh(app *a, const ctw_channel *ch, const ctw_airing *air, int64_t start, asp_rect r)
{
    memset(&info, 0, sizeof info);
    info.valid = true;
    info.channel = ch->number;
    info.start = start;
    info.air = air;
    float left_w = r.x + DIVIDER_X - DIVIDER_GAP - text_x(r);
    ui_text_fit(FONT_BOLD, NAME_PX, ch->name, left_w, info.name, sizeof info.name);
    if (air == NULL) return;

    const ctw_airing *next = air->kind == CTW_AIR_UPNEXT ? tn_next(&a->tuner, ch->number, air) : NULL;
    info.icon_show = gt_line_show(air, next);

    char buf[TF_LINE_BYTES];
    gt_line(air, next, buf, sizeof buf);
    ui_text_fit(FONT_BOLD, LINE_PX, buf, left_w, info.line, sizeof info.line);
    gt_times(air->start_ms, air->duration_ms, info.times, sizeof info.times);
    info.times_w = ui_text_width(FONT_BODY, TIMES_PX, info.times);

    info.next_ep = tn_next_episode(&a->tuner, ch->number, air);
    if (info.next_ep != NULL && info.next_ep->episode != NULL) {
        gt_episode(info.next_ep->episode, buf, sizeof buf);
        info.next_count = ui_text_wrap(FONT_BOLD, NEXT_TITLE_PX, buf, NEXT_W, NEXT_LINES, info.next);
    }
}

void ginfo_draw_info(app *a, const ctw_channel *ch, asp_rect r)
{
    ui_card(r.x, r.y, r.w, r.h, THEME_PANEL_RADIUS, 0.6f, THEME_GLOSS_BOTTOM, UI_SHADOW_PANEL);
    if (ch == NULL) return;
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, ch->number, a->today, a->ms_of_day, &offset);
    int64_t start = air != NULL ? tn_airing_start(&a->tuner, ch->number, air) : -1;
    if (!info.valid || info.channel != ch->number || info.start != start || info.air != air) refresh(a, ch, air, start, r);

    char path[320];
    gt_icon_path(a->root, ch, info.icon_show, path, sizeof path);
    float iw = ICON_PX * ui_pixel_x();
    float band = r.h < GL_INFO_MIN_H ? r.h : GL_INFO_MIN_H;
    icons_draw(path, r.x + ICON_X - (ICON_PX - iw) / 2.0f, r.y + (band - ICON_PX) / 2.0f, ICON_PX);

    float tx = text_x(r);
    ui_text(FONT_BOLD, NAME_PX, tx, r.y + NAME_Y, info.name, THEME_ACCENT, ALIGN_LEFT);
    if (air == NULL) return;
    ui_text(FONT_BOLD, LINE_PX, tx, r.y + LINE_Y, info.line, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, TIMES_PX, tx, r.y + TIMES_Y, info.times, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    float bx = tx + info.times_w + BAR_GAP;
    ui_bar(bx, r.y + BAR_Y, r.x + BAR_RIGHT - bx, BAR_H, gt_progress(air, offset));

    GRRLIB_Rectangle(r.x + DIVIDER_X, r.y + DIVIDER_Y, 1.0f, DIVIDER_H, THEME_BAR_TRACK, 1);
    float nx = r.x + NEXT_X;
    ui_text(FONT_BOLD, NEXT_LABEL_PX, nx, r.y + NEXT_LABEL_Y, "Up next", THEME_ACCENT, ALIGN_LEFT);
    for (size_t i = 0; i < info.next_count; i++) {
        ui_text(FONT_BOLD, NEXT_TITLE_PX, nx, r.y + NEXT_TITLE_Y + NEXT_LINE_H * (float)i, info.next[i].text, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    }
    if (info.next_ep != NULL) {
        char countdown[16];
        int64_t until = tn_ms_until(&a->tuner, ch->number, air, offset, info.next_ep);
        gt_countdown((uint32_t)(until > 0xFFFFFFFFLL ? 0xFFFFFFFFu : until), countdown, sizeof countdown);
        ui_text(FONT_BOLD, COUNTDOWN_PX, nx, r.y + COUNTDOWN_Y, countdown, THEME_ACCENT, ALIGN_LEFT);
    }
}
