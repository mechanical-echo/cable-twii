#include "panel.h"

#include <stdio.h>
#include <string.h>

#include "../guide_text.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define NAME_PX 16
#define LINE_PX 14
#define TIMES_PX 12
#define SEGMENT_RADIUS 3.0f

typedef struct {
    bool valid;
    int channel;
    int64_t start;
    char name[TF_LINE_BYTES];
    char line[TF_LINE_BYTES];
    char times[32];
    const ctw_show *icon_show;
} panel_text;

static panel_text text;

static void refresh(app *a, const osd_layout *l, const ctw_channel *ch, const ctw_airing *air, int64_t start)
{
    memset(&text, 0, sizeof text);
    text.valid = true;
    text.channel = ch->number;
    text.start = start;
    ui_text_fit(FONT_BOLD, NAME_PX, ch->name, l->name.w, text.name, sizeof text.name);
    if (air == NULL) return;
    const ctw_airing *next = air->kind == CTW_AIR_UPNEXT ? tn_next(&a->tuner, ch->number, air) : NULL;
    text.icon_show = gt_line_show(air, next);
    char buf[TF_LINE_BYTES];
    gt_line(air, next, buf, sizeof buf);
    ui_text_fit(FONT_BOLD, LINE_PX, buf, l->line.w, text.line, sizeof text.line);
    gt_times(air->start_ms, air->duration_ms, text.times, sizeof text.times);
}

static void segments(asp_rect bar, int volume)
{
    int lit = osd_segments_lit(volume);
    for (int i = 0; i < OSD_VOLUME_SEGMENTS; i++) {
        asp_rect s = osd_segment(bar, i);
        ui_rounded(s.x, s.y, s.w, s.h, SEGMENT_RADIUS, i < lit ? THEME_ACCENT : THEME_BAR_TRACK);
    }
}

static void pill(asp_rect r, const char *label, bool hot)
{
    ui_pill(r.x, r.y, r.w, r.h, label, hot);
}

void panel_draw(app *a, const osd_layout *l, osd_btn hot)
{
    ui_panel(l->panel.x, l->panel.y, l->panel.w, l->panel.h, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);

    const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
    if (ch != NULL) {
        uint32_t offset = 0;
        const ctw_airing *air = tn_current(&a->tuner, ch->number, a->today, a->ms_of_day, &offset);
        int64_t start = air != NULL ? tn_airing_start(&a->tuner, ch->number, air) : -1;
        if (!text.valid || text.channel != ch->number || text.start != start) refresh(a, l, ch, air, start);

        ui_text(FONT_BOLD, NAME_PX, l->name.x, l->name.y, text.name, THEME_ACCENT, ALIGN_LEFT);
        if (air != NULL) {
            char path[320];
            gt_icon_path(a->root, ch, text.icon_show, path, sizeof path);
            icons_draw(path, l->icon.x, l->icon.y, l->icon.w);
            ui_text(FONT_BOLD, LINE_PX, l->line.x, l->line.y, text.line, THEME_TEXT_PRIMARY, ALIGN_LEFT);
            ui_text(FONT_BODY, TIMES_PX, l->times.x, l->times.y, text.times, THEME_TEXT_SECONDARY, ALIGN_LEFT);
            ui_bar(l->bar.x, l->bar.y, l->bar.w, l->bar.h, gt_progress(air, offset));
        }
    }

    pill(l->prev, "\xE2\x97\x80 CH", hot == OSD_BTN_PREV);
    pill(l->next, "CH \xE2\x96\xB6", hot == OSD_BTN_NEXT);
    pill(l->guide, "Guide", hot == OSD_BTN_GUIDE);
    pill(l->vol_down, "\xE2\x88\x92", hot == OSD_BTN_VOL_DOWN);
    segments(l->vol_bar, a->volume);
    pill(l->vol_up, "+", hot == OSD_BTN_VOL_UP);
}

void panel_draw_volume(const osd_layout *l, int volume)
{
    ui_panel(l->vol_panel.x, l->vol_panel.y, l->vol_panel.w, l->vol_panel.h, l->vol_panel.h / 2.0f, THEME_PANEL_FILL, THEME_PANEL_BORDER);
    ui_text(FONT_BOLD, LINE_PX, l->vol_panel.x + 20.0f, l->vol_panel.y + l->vol_panel.h / 2.0f - 9.0f, "Volume", THEME_ACCENT, ALIGN_LEFT);
    segments(l->vol_panel_bar, volume);
}
