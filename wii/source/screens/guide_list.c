#include "guide_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grrlib.h>

#include "../guide_text.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define ICON_PX 44.0f
#define LINE_ICON_PX 20.0f
#define TEXT_X 62.0f
#define ROW_PAD 8.0f
#define NAME_PX 16
#define LINE_PX 13
#define SMALL_PX 12
#define HEADER_PX 14
#define BAR_H 4.0f

typedef struct {
    bool valid;
    int64_t start;
    char name[TF_LINE_BYTES];
    char number[8];
    char line[TF_LINE_BYTES];
    char times[32];
    float times_w;
    const ctw_show *line_show;
} row_text;

static row_text *texts;
static size_t text_count;

int glist_init(size_t row_count)
{
    free(texts);
    text_count = 0;
    texts = row_count > 0 ? (row_text *)calloc(row_count, sizeof *texts) : NULL;
    if (row_count > 0 && texts == NULL) return -1;
    text_count = row_count;
    return 0;
}

void glist_view(asp_rect panel, float *top, float *height)
{
    *top = panel.y + GLIST_PAD;
    *height = panel.h - 2.0f * GLIST_PAD;
}

static void refresh(app *a, row_text *t, const ctw_channel *ch, const ctw_airing *air, int64_t start, float w)
{
    t->valid = true;
    t->start = start;
    snprintf(t->number, sizeof t->number, "%d", ch->number);
    float text_w = w - TEXT_X - ROW_PAD;
    float num_w = ui_text_width(FONT_BODY, SMALL_PX, t->number);
    ui_text_fit(FONT_BOLD, NAME_PX, ch->name, text_w - num_w - 6.0f, t->name, sizeof t->name);

    const ctw_airing *next = air != NULL && air->kind == CTW_AIR_UPNEXT ? tn_next(&a->tuner, ch->number, air) : NULL;
    t->line_show = gt_line_show(air, next);
    char line[TF_LINE_BYTES];
    gt_line(air, next, line, sizeof line);
    float line_w = text_w - (t->line_show != NULL ? LINE_ICON_PX + 6.0f : 0.0f);
    ui_text_fit(FONT_BODY, LINE_PX, line, line_w, t->line, sizeof t->line);

    if (air != NULL) {
        gt_times(air->start_ms, air->duration_ms, t->times, sizeof t->times);
    } else {
        t->times[0] = '\0';
    }
    t->times_w = ui_text_width(FONT_BODY, SMALL_PX, t->times);
}

static void draw_header(const gm_row *r, float x, float y)
{
    ui_text(FONT_BOLD, HEADER_PX, x + 6.0f, y + 7.0f, r->category != NULL ? r->category : "", THEME_TEXT_SECONDARY, ALIGN_LEFT);
}

static void draw_channel(app *a, row_text *t, const gm_row *r, bool hot, float x, float y, float w)
{
    const ctw_channel *ch = r->channel;
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, ch->number, a->today, a->ms_of_day, &offset);
    int64_t start = air != NULL ? tn_airing_start(&a->tuner, ch->number, air) : -1;
    if (!t->valid || t->start != start) refresh(a, t, ch, air, start, w);

    if (hot) {
        ui_glow(x, y, w, r->h, THEME_ROW_RADIUS, 0.25f);
        ui_panel(x, y, w, r->h, THEME_ROW_RADIUS, THEME_ROW_ACTIVE_FILL, THEME_ACCENT);
    }

    char path[320];
    gt_icon_path(a->root, ch, NULL, path, sizeof path);
    icons_draw(path, x + ROW_PAD, y + (r->h - ICON_PX) / 2.0f, ICON_PX);

    float tx = x + TEXT_X;
    ui_text(FONT_BOLD, NAME_PX, tx, y + 5.0f, t->name, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, SMALL_PX, x + w - ROW_PAD, y + 7.0f, t->number, THEME_TEXT_SECONDARY, ALIGN_RIGHT);

    float lx = tx;
    if (t->line_show != NULL) {
        gt_icon_path(a->root, ch, t->line_show, path, sizeof path);
        icons_draw(path, tx, y + 24.0f, LINE_ICON_PX);
        lx += LINE_ICON_PX + 6.0f;
    }
    ui_text(FONT_BODY, LINE_PX, lx, y + 26.0f, t->line, THEME_TEXT_SECONDARY, ALIGN_LEFT);

    if (air == NULL) return;
    ui_text(FONT_BODY, SMALL_PX, tx, y + 45.0f, t->times, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    float bx = tx + t->times_w + 8.0f;
    ui_bar(bx, y + 51.0f, x + w - ROW_PAD - bx, BAR_H, gt_progress(air, offset));
}

void glist_draw(app *a, const gm_row *rows, size_t n, int highlight, float scroll, asp_rect panel)
{
    ui_panel(panel.x, panel.y, panel.w, panel.h, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);
    float top, view_h;
    glist_view(panel, &top, &view_h);
    float x = panel.x + GLIST_PAD, w = panel.w - 2.0f * GLIST_PAD;

    if (n == 0) {
        ui_text(FONT_BODY, 14, panel.x + panel.w / 2.0f, top + 20.0f, "No channels yet", THEME_TEXT_SECONDARY, ALIGN_CENTER);
        return;
    }

    float cx = panel.x + 2.0f, cy = panel.y + 2.0f;
    GRRLIB_ClipDrawing((u32)(cx < 0.0f ? 0.0f : cx), (u32)cy, (u32)(panel.w - 4.0f), (u32)(panel.h - 4.0f));
    for (size_t i = 0; i < n; i++) {
        const gm_row *r = &rows[i];
        if (r->y + r->h < scroll - GLIST_PAD || r->y > scroll + view_h + GLIST_PAD) continue;
        float y = top + r->y - scroll;
        if (r->kind == GM_HEADER) {
            draw_header(r, x, y);
        } else if (i < text_count) {
            draw_channel(a, &texts[i], r, (int)i == highlight, x, y, w);
        }
    }
    GRRLIB_ClipReset();
}
