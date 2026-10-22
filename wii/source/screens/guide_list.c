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

#define ROW_INSET_LEFT 6.7f
#define ROW_INSET_RIGHT 14.0f
#define ICON_PX 44.0f
#define ICON_X 7.3f
#define TEXT_GAP 7.3f
#define NAME_PX 16
#define NAME_Y 13.4f
#define LINE_PX 13
#define LINE_Y 34.0f
#define NUMBER_PX 12
#define NUMBER_H 19.0f
#define NUMBER_INSET 3.3f
#define NUMBER_GAP 6.0f
#define BAR_Y 52.6f
#define BAR_H 4.4f
#define BAR_GAP 7.3f
#define HEADER_PX 12
#define HEADER_INSET 6.7f
#define HEADER_Y 8.0f
#define RULE_GAP 6.0f
#define RULE_DY 6.5f
#define SCROLL_W 3.0f
#define SCROLL_RIGHT 9.3f
#define SCROLL_INSET 17.0f
#define SCROLL_MIN_THUMB 20.0f
#define HOT_SHADOW_DY 3.0f
#define HOT_SHADOW_BLUR 7.0f

typedef struct {
    bool valid;
    int64_t start;
    char name[TF_LINE_BYTES];
    char number[8];
    char line[TF_LINE_BYTES];
    float number_w;
} row_text;

static const ui_stop HOT_STOPS[] = { { 0.0f, THEME_ROW_HOT_TOP }, { 0.55f, THEME_ROW_HOT_MID }, { 1.0f, THEME_ROW_HOT_BOTTOM } };

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

static float icon_w(void)
{
    return ICON_PX * ui_pixel_x();
}

static void refresh(app *a, row_text *t, const ctw_channel *ch, const ctw_airing *air, int64_t start, float w)
{
    t->valid = true;
    t->start = start;
    gt_number(ch->number, t->number, sizeof t->number);
    t->number_w = ui_number_pill_w(t->number, NUMBER_PX);
    float text_w = w - (ICON_X + icon_w() + TEXT_GAP) - NUMBER_INSET - t->number_w - NUMBER_GAP;
    ui_text_fit(FONT_BOLD, NAME_PX, ch->name, text_w, t->name, sizeof t->name);
    const ctw_airing *next = air != NULL && air->kind == CTW_AIR_UPNEXT ? tn_next(&a->tuner, ch->number, air) : NULL;
    char line[TF_LINE_BYTES];
    gt_line(air, next, line, sizeof line);
    ui_text_fit(FONT_BODY, LINE_PX, line, text_w, t->line, sizeof t->line);
}

static void upper_ascii(char *s)
{
    for (; *s != '\0'; s++) {
        if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
    }
}

static void draw_header(row_text *t, const gm_row *r, float x, float y, float w)
{
    if (!t->valid) {
        t->valid = true;
        ui_text_fit(FONT_BOLD, HEADER_PX, r->category != NULL ? r->category : "", w - 2.0f * HEADER_INSET, t->name, sizeof t->name);
        upper_ascii(t->name);
    }
    ui_text(FONT_BOLD, HEADER_PX, x + HEADER_INSET, y + HEADER_Y, t->name, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    float rx = x + HEADER_INSET + ui_text_width(FONT_BOLD, HEADER_PX, t->name) + RULE_GAP;
    if (rx < x + w) GRRLIB_Rectangle(rx, y + HEADER_Y + RULE_DY, x + w - rx, 1.0f, THEME_BAR_TRACK, 1);
}

static void draw_channel(app *a, row_text *t, const gm_row *r, bool hot, bool tuned, float x, float y, float w)
{
    const ctw_channel *ch = r->channel;
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, ch->number, a->today, a->ms_of_day, &offset);
    int64_t start = air != NULL ? tn_airing_start(&a->tuner, ch->number, air) : -1;
    if (!t->valid || t->start != start) refresh(a, t, ch, air, start, w);

    if (hot) {
        ui_shadow(x, y, w, r->h, THEME_ROW_RADIUS, THEME_ROW_HOT_SHADOW, HOT_SHADOW_DY, HOT_SHADOW_BLUR);
        ui_ring(x, y, w, r->h, THEME_ROW_RADIUS, THEME_RING, THEME_RING_SPREAD);
        ui_outline(x, y, w, r->h, THEME_ROW_RADIUS, 2.0f, THEME_ACCENT);
        float ix = x + 2.0f * ui_pixel_x(), iy = y + 2.0f, iw = w - 4.0f * ui_pixel_x(), ih = r->h - 4.0f;
        ui_rounded(ix, iy, iw, ih, THEME_ROW_RADIUS - 2.0f, THEME_PANEL_HIGHLIGHT);
        ui_rounded_gradient(ix, iy + 1.0f, iw, ih - 1.0f, THEME_ROW_RADIUS - 2.0f, HOT_STOPS, 3);
    }

    char path[320];
    gt_icon_path(a->root, ch, NULL, path, sizeof path);
    float iw = icon_w();
    float icon_x = x + ICON_X;
    icons_draw(path, icon_x - (ICON_PX - iw) / 2.0f, y + (r->h - ICON_PX) / 2.0f, ICON_PX);

    float tx = icon_x + iw + TEXT_GAP;
    ui_text(FONT_BOLD, NAME_PX, tx, y + NAME_Y, t->name, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, LINE_PX, tx, y + LINE_Y, t->line, hot ? THEME_TEXT_LABEL : THEME_TEXT_SECONDARY, ALIGN_LEFT);

    float px = x + w - NUMBER_INSET - t->number_w;
    ui_number_pill(px, y + (r->h - NUMBER_H) / 2.0f, NUMBER_H, t->number, NUMBER_PX, tuned);

    if (hot && air != NULL) ui_bar(tx, y + BAR_Y, px - BAR_GAP - tx, BAR_H, gt_progress(air, offset));
}

static void draw_scrollbar(asp_rect panel, float view_h, float content_h, float scroll)
{
    if (content_h <= view_h) return;
    float x = panel.x + panel.w - SCROLL_RIGHT;
    float y = panel.y + SCROLL_INSET, h = panel.h - 2.0f * SCROLL_INSET;
    ui_rounded(x, y, SCROLL_W, h, SCROLL_W / 2.0f, THEME_BAR_TRACK);
    float th = h * view_h / content_h;
    if (th < SCROLL_MIN_THUMB) th = SCROLL_MIN_THUMB;
    float ty = y + (h - th) * (scroll / (content_h - view_h));
    ui_rounded(x, ty, SCROLL_W, th, SCROLL_W / 2.0f, THEME_PILL_BORDER);
}

void glist_draw(app *a, const gm_row *rows, size_t n, int highlight, float scroll, asp_rect panel, int tuned_channel)
{
    ui_card(panel.x, panel.y, panel.w, panel.h, THEME_PANEL_RADIUS, 0.7f, THEME_GLOSS_BOTTOM, UI_SHADOW_PANEL);
    float top, view_h;
    glist_view(panel, &top, &view_h);
    float x = panel.x + ROW_INSET_LEFT, w = panel.w - ROW_INSET_LEFT - ROW_INSET_RIGHT;
    if (n == 0) return;

    float cx = panel.x + 2.0f, cy = panel.y + 2.0f;
    GRRLIB_ClipDrawing((u32)(cx < 0.0f ? 0.0f : cx), (u32)cy, (u32)(panel.w - 4.0f), (u32)(panel.h - 4.0f));
    for (size_t i = 0; i < n; i++) {
        const gm_row *r = &rows[i];
        if (r->y + r->h < scroll - GLIST_PAD || r->y > scroll + view_h + GLIST_PAD) continue;
        float y = top + r->y - scroll;
        if (i >= text_count) continue;
        if (r->kind == GM_HEADER) {
            draw_header(&texts[i], r, x, y, w);
        } else {
            bool tuned = tuned_channel != 0 && r->channel->number == tuned_channel;
            draw_channel(a, &texts[i], r, (int)i == highlight, tuned, x, y, w);
        }
    }
    GRRLIB_ClipReset();
    draw_scrollbar(panel, view_h, gm_content_h(rows, n), scroll);
}
