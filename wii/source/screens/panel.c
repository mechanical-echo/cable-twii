#include "panel.h"

#include <stdio.h>
#include <string.h>
#include <grrlib.h>

#include "../guide_text.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define NAME_PX 18
#define NAME_MAX_W 230.0f
#define NUMBER_PX 11
#define LINE_PX 16
#define TIMES_PX 12
#define NEXT_LABEL_PX 13
#define NEXT_TITLE_PX 13
#define NEXT_LINE_H 16.0f
#define NEXT_LINES 2
#define NEXT_AT_PX 12
#define COUNTDOWN_PX 28
#define VOL_LABEL_PX 13
#define VOL_BTN_PX 20
#define VOL_PANEL_MID 0.55f
#define PANEL_ALPHA 0.97f
#define ICON_PX 48.0f
#define NEXT_ICON_PX 40.0f

typedef struct {
    bool valid;
    int channel;
    int64_t start;
    char name[TF_LINE_BYTES];
    float name_w;
    char number[8];
    char line[TF_LINE_BYTES];
    char start_text[8];
    char end_text[8];
    float start_w;
    tf_line next[NEXT_LINES];
    size_t next_count;
    char next_at[16];
    const ctw_show *icon_show;
    const ctw_airing *next_ep;
} panel_text;

static panel_text text;

static void refresh(app *a, const ctw_channel *ch, const ctw_airing *air, int64_t start)
{
    memset(&text, 0, sizeof text);
    text.valid = true;
    text.channel = ch->number;
    text.start = start;
    ui_text_fit(FONT_BOLD, NAME_PX, ch->name, NAME_MAX_W, text.name, sizeof text.name);
    text.name_w = ui_text_width(FONT_BOLD, NAME_PX, text.name);
    gt_number(ch->number, text.number, sizeof text.number);
    if (air == NULL) return;
    const ctw_airing *next = air->kind == CTW_AIR_UPNEXT ? tn_next(&a->tuner, ch->number, air) : NULL;
    text.icon_show = gt_line_show(air, next);
    osd_layout probe = osd_layout_for(0.0f, ui_pixel_x(), text.name_w, 0.0f);
    char buf[TF_LINE_BYTES];
    gt_line(air, next, buf, sizeof buf);
    ui_text_fit(FONT_BOLD, LINE_PX, buf, probe.line.w, text.line, sizeof text.line);
    gt_hhmm(air->start_ms, text.start_text, sizeof text.start_text);
    gt_hhmm(air->start_ms + (int64_t)air->duration_ms, text.end_text, sizeof text.end_text);
    text.start_w = ui_text_width(FONT_BODY, TIMES_PX, text.start_text);
    text.next_ep = tn_next_episode(&a->tuner, ch->number, air);
    if (text.next_ep != NULL && text.next_ep->episode != NULL) {
        gt_episode(text.next_ep->episode, buf, sizeof buf);
        text.next_count = ui_text_wrap(FONT_BOLD, NEXT_TITLE_PX, buf, probe.next_title.w, NEXT_LINES, text.next);
        gt_at(text.next_ep->start_ms, text.next_at, sizeof text.next_at);
    }
}

static const ctw_airing *current(app *a, const ctw_channel **ch, uint32_t *offset)
{
    *ch = ctw_db_channel(a->db, a->current_channel);
    if (*ch == NULL) return NULL;
    const ctw_airing *air = tn_current(&a->tuner, (*ch)->number, a->today, a->ms_of_day, offset);
    int64_t start = air != NULL ? tn_airing_start(&a->tuner, (*ch)->number, air) : -1;
    if (!text.valid || text.channel != (*ch)->number || text.start != start) refresh(a, *ch, air, start);
    return air;
}

osd_layout panel_layout(app *a, float offset_y)
{
    const ctw_channel *ch;
    uint32_t offset = 0;
    current(a, &ch, &offset);
    return osd_layout_for(offset_y, ui_pixel_x(), text.valid ? text.name_w : 0.0f, text.valid ? text.start_w : 0.0f);
}

void panel_draw(app *a, const osd_layout *l)
{
    asp_rect scrim = osd_scrim();
    ui_gradient_rect(scrim.x, scrim.y, scrim.w, scrim.h, 0x00000000u, THEME_SCRIM);
    ui_card_alpha(l->panel.x, l->panel.y, l->panel.w, l->panel.h, THEME_PANEL_RADIUS, 0.55f, THEME_GLOSS_SOFT, UI_SHADOW_VIDEO, PANEL_ALPHA);

    const ctw_channel *ch;
    uint32_t offset = 0;
    const ctw_airing *air = current(a, &ch, &offset);
    if (ch == NULL) return;

    char path[320];
    gt_icon_path(a->root, ch, text.icon_show, path, sizeof path);
    icons_draw(path, l->icon.x - (ICON_PX - l->icon.w) / 2.0f, l->icon.y, ICON_PX);
    ui_text(FONT_BOLD, NAME_PX, l->name.x, l->name.y, text.name, THEME_ACCENT, ALIGN_LEFT);
    ui_number_pill(l->number.x, l->number.y, l->number.h, text.number, NUMBER_PX, false);
    if (air == NULL) return;

    ui_text(FONT_BOLD, LINE_PX, l->line.x, l->line.y, text.line, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, TIMES_PX, l->start.x, l->start.y, text.start_text, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    ui_bar(l->bar.x, l->bar.y, l->bar.w, l->bar.h, gt_progress(air, offset));
    ui_text(FONT_BODY, TIMES_PX, l->end.x + l->end.w, l->end.y, text.end_text, THEME_TEXT_SECONDARY, ALIGN_RIGHT);
    GRRLIB_Rectangle(l->divider.x, l->divider.y, l->divider.w, l->divider.h, THEME_BAR_TRACK, 1);

    if (text.next_ep != NULL) {
        gt_icon_path(a->root, ch, text.next_ep->show, path, sizeof path);
        icons_draw(path, l->next_icon.x - (NEXT_ICON_PX - l->next_icon.w) / 2.0f, l->next_icon.y, NEXT_ICON_PX);
    }
    ui_text(FONT_BOLD, NEXT_LABEL_PX, l->next_label.x, l->next_label.y, "Up next", THEME_ACCENT, ALIGN_LEFT);
    for (size_t i = 0; i < text.next_count; i++) {
        ui_text(FONT_BOLD, NEXT_TITLE_PX, l->next_title.x, l->next_title.y + NEXT_LINE_H * (float)i, text.next[i].text, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    }
    if (text.next_ep != NULL) {
        ui_text(FONT_BODY, NEXT_AT_PX, l->next_at.x, l->next_at.y, text.next_at, THEME_TEXT_SECONDARY, ALIGN_LEFT);
        char countdown[16];
        int64_t until = tn_ms_until(&a->tuner, ch->number, air, offset, text.next_ep);
        gt_countdown((uint32_t)(until > 0xFFFFFFFFLL ? 0xFFFFFFFFu : until), countdown, sizeof countdown);
        ui_text(FONT_BOLD, COUNTDOWN_PX, l->countdown.x + l->countdown.w, l->countdown.y, countdown, THEME_ACCENT, ALIGN_RIGHT);
    }
}

static ui_pill_state state_for(osd_btn btn, osd_btn hot, osd_btn pressed)
{
    if (pressed == btn) return UI_PILL_PRESSED;
    return hot == btn ? UI_PILL_HOT : UI_PILL_REST;
}

void panel_draw_volume(const osd_layout *l, int volume, osd_btn hot, osd_btn pressed)
{
    ui_text(FONT_BOLD, VOL_LABEL_PX, l->vol_label.x + l->vol_label.w / 2.0f, l->vol_label.y, "Volume", THEME_PANEL_FILL, ALIGN_CENTER);
    ui_card(l->vol_panel.x, l->vol_panel.y, l->vol_panel.w, l->vol_panel.h, l->vol_panel.h / 2.0f, VOL_PANEL_MID, THEME_GLOSS_COOL, UI_SHADOW_VIDEO);
    ui_pill_ex(l->vol_down.x, l->vol_down.y, l->vol_down.w, l->vol_down.h, "\xE2\x88\x92", VOL_BTN_PX, state_for(OSD_BTN_VOL_DOWN, hot, pressed), UI_SHADOW_NONE);
    ui_segments(l->vol_bar.x, l->vol_bar.y, l->vol_bar.w, l->vol_bar.h, OSD_VOLUME_SEGMENTS, 3.0f, osd_segments_lit(volume), true);
    ui_pill_ex(l->vol_up.x, l->vol_up.y, l->vol_up.w, l->vol_up.h, "+", VOL_BTN_PX, state_for(OSD_BTN_VOL_UP, hot, pressed), UI_SHADOW_NONE);
}
