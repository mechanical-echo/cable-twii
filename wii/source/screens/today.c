#include "today.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "../guide_model.h"
#include "../guide_text.h"
#include "../input.h"
#include "../live.h"
#include "../today_layout.h"
#include "../today_model.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"
#include "../ui/tween.h"
#include "full.h"

#define SCROLL_MS 200u
#define CARD_MID 0.85f
#define ICON_PX 36.0f
#define NAME_PX 18
#define NUMBER_PX 11
#define NUMBER_H 17.0f
#define NUMBER_GAP 6.0f
#define NUMBER_DY 1.0f
#define DATE_PX 12
#define TAG_PX 12
#define TAG_PAD 8.0f
#define TIME_PX 12
#define SHOW_PX 14
#define CODE_PX 12
#define TITLE_PX 13
#define NOTE_PX 12
#define ROW_RADIUS 10.0f
#define ROW_ICON_PX 22.0f
#define TIME_DY 9.0f
#define SHOW_DY 7.0f
#define CODE_DY 9.0f
#define TITLE_DY 8.0f
#define ICON_DY 5.0f
#define EDGE_X 3.0f
#define EDGE_Y 6.0f
#define EDGE_W 4.0f
#define BAR_X 12.0f
#define BAR_DY 6.0f
#define BAR_H 3.0f
#define CLIP_SPREAD 6.0f
#define HOT_SHADOW_DY 3.0f
#define HOT_SHADOW_BLUR 7.0f
#define SCROLL_MIN_THUMB 20.0f
#define KEY_GAP 5.0f
#define HINT_GAP 22.0f
#define WIDEST_TIME "23:30 \xE2\x80\x93 00:45 +1"
#define WIDEST_CODE "S00E00"

typedef struct {
    bool valid;
    char time[32];
    char show[TF_LINE_BYTES];
    char code[16];
    char title[TF_LINE_BYTES];
} row_text;

static const ui_stop HOT_STOPS[] = { { 0.0f, THEME_ROW_HOT_TOP }, { 0.55f, THEME_ROW_HOT_MID }, { 1.0f, THEME_ROW_HOT_BOTTOM } };
static const ui_stop DISC_STOPS[] = { { 0.0f, THEME_PANEL_FILL }, { 1.0f, THEME_HINT_DISC_BOTTOM } };

static td_row *rows;
static row_text *texts;
static size_t row_count;
static bool built;
static int channel;
static const ctw_airing *items;
static uint32_t day_key;
static int64_t shift;
static int highlight = -1;
static int last_hit = -1;
static float scroll;
static tween scroll_tw;
static bool scrolling;
static today_layout layout;
static float time_w;
static float code_w;
static char header_name[TF_LINE_BYTES];
static char header_number[8];
static char header_date[32];

static void free_rows(void)
{
    free(rows);
    free(texts);
    rows = NULL;
    texts = NULL;
    row_count = 0;
    built = false;
    items = NULL;
}

void today_reset(app *a)
{
    free_rows();
    highlight = -1;
    last_hit = -1;
    scrolling = false;
    a->today_open = false;
}

static bool looping(const app *a)
{
    const ctw_channel *ch = ctw_db_channel(a->db, channel);
    return ch != NULL && ch->mode == CTW_MODE_LOOPING;
}

static const ctw_airing *now_airing(app *a, uint32_t *offset)
{
    return tn_current(&a->tuner, channel, a->today, a->ms_of_day, offset);
}

static int now_row(app *a)
{
    uint32_t offset = 0;
    return td_row_of(rows, row_count, now_airing(a, &offset));
}

static bool rebuild(app *a)
{
    ctw_date day = a->today;
    const ctw_lineup *l = tn_lineup_now(&a->tuner, channel, a->today, a->ms_of_day, &day);
    const ctw_airing *its = l != NULL ? l->items : NULL;
    if (built && its == items && ctw_date_key(day) == day_key) return false;
    free_rows();
    built = true;
    items = its;
    day_key = ctw_date_key(day);
    gt_today(day, header_date, sizeof header_date);
    size_t n = l != NULL ? l->count : 0;
    if (n == 0) return true;
    rows = (td_row *)malloc(n * sizeof *rows);
    texts = (row_text *)calloc(n, sizeof *texts);
    if (rows == NULL || texts == NULL) {
        free_rows();
        built = true;
        items = its;
        return true;
    }
    row_count = td_build(l, rows, n);
    return true;
}

static void place_at_now(app *a)
{
    int now = now_row(a);
    highlight = now >= 0 ? now : (row_count > 0 ? 0 : -1);
    scroll = td_open_scroll(now, row_count, layout.list.h);
    scrolling = false;
}

void today_open(app *a, int channel_number)
{
    today_reset(a);
    channel = channel_number;
    layout = today_layout_for(ui_pixel_x());
    time_w = ui_text_width(FONT_BODY, TIME_PX, WIDEST_TIME);
    code_w = ui_text_width(FONT_BOLD, CODE_PX, WIDEST_CODE);
    gt_number(channel, header_number, sizeof header_number);
    const ctw_channel *ch = ctw_db_channel(a->db, channel);
    float name_w = layout.name.w - NUMBER_GAP - ui_number_pill_w(header_number, NUMBER_PX);
    ui_text_fit(FONT_BOLD, NAME_PX, ch != NULL ? ch->name : "", name_w, header_name, sizeof header_name);
    shift = tn_shift(&a->tuner, channel);
    rebuild(a);
    place_at_now(a);
    a->today_open = true;
    sfx_play(SFX_SELECT);
}

static void close_list(app *a)
{
    sfx_play(SFX_BACK);
    today_reset(a);
}

static void move(app *a, int dir)
{
    if (row_count == 0) return;
    int next = highlight + dir;
    if (next < 0) next = 0;
    if ((size_t)next >= row_count) next = (int)row_count - 1;
    if (next == highlight) return;
    highlight = next;
    sfx_play(SFX_HOVER);
    float target = td_scroll_to(next, scroll, layout.list.h, row_count);
    if (target == scroll) return;
    scroll_tw = tw_make(scroll, target, a->now_ms, SCROLL_MS);
    scrolling = true;
}

static void play(app *a)
{
    if (!looping(a) || highlight < 0 || (size_t)highlight >= row_count) return;
    int ch = channel;
    if (!tn_jump(&a->tuner, ch, a->today, a->ms_of_day, rows[highlight].first)) return;
    screen_id from = a->screen;
    today_reset(a);
    if (from == SCR_GUIDE) {
        sfx_play(SFX_SELECT);
        live_tune(a, ch);
        app_go(a, SCR_FULL);
    } else if (ch == a->current_channel) {
        full_jumped(a);
    }
}

static int pointer_row(void)
{
    if (!cur_visible()) return -1;
    float px = cur_x(), py = cur_y();
    asp_rect l = layout.list;
    if (px < l.x || px > l.x + l.w) return -1;
    float next = gm_edge_scroll(scroll, py, l.y, l.h, td_content_h(row_count));
    if (next != scroll) {
        scroll = next;
        scrolling = false;
    }
    if (py < l.y || py > l.y + l.h) return -1;
    return td_hit(scroll, py - l.y, row_count);
}

void today_update(app *a)
{
    if (rebuild(a)) place_at_now(a);
    if (in_pressed(WPAD_BUTTON_B) || in_pressed(WPAD_BUTTON_1)) {
        close_list(a);
        return;
    }
    if (scrolling) {
        scroll = tw_value(&scroll_tw, a->now_ms);
        if (tw_done(&scroll_tw, a->now_ms)) scrolling = false;
    }
    if (in_pressed(WPAD_BUTTON_DOWN)) move(a, 1);
    else if (in_pressed(WPAD_BUTTON_UP)) move(a, -1);

    int hit = pointer_row();
    if (hit >= 0 && hit != last_hit && hit != highlight) {
        highlight = hit;
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    last_hit = hit;

    if (in_pressed(WPAD_BUTTON_A) && (!cur_visible() || hit >= 0)) play(a);
}

static void refresh(const td_row *r, row_text *t, const today_cols *c, float row_right)
{
    t->valid = true;
    gt_span(r->start_ms - shift, r->end_ms - shift, t->time, sizeof t->time);
    if (r->kind == TD_ADS) {
        char buf[TF_LINE_BYTES];
        gt_ad_break(r->count, r->total_ms, buf, sizeof buf);
        ui_text_fit(FONT_BODY, TITLE_PX, buf, row_right - c->show_x, t->title, sizeof t->title);
        return;
    }
    if (r->kind == TD_UPNEXT || r->first->show == NULL || r->first->episode == NULL) {
        snprintf(t->title, sizeof t->title, "Up next");
        return;
    }
    const ctw_episode *ep = r->first->episode;
    ui_text_fit(FONT_BOLD, SHOW_PX, r->first->show->name, c->show_w, t->show, sizeof t->show);
    snprintf(t->code, sizeof t->code, "S%02uE%02u", (unsigned)ep->season, (unsigned)ep->episode);
    ui_text_fit(FONT_BODY, TITLE_PX, ep->title != NULL ? ep->title : "", c->title_w, t->title, sizeof t->title);
}

static void draw_hot(float x, float y, float w, float h)
{
    float px = ui_pixel_x();
    ui_shadow(x, y, w, h, ROW_RADIUS, THEME_ROW_HOT_SHADOW, HOT_SHADOW_DY, HOT_SHADOW_BLUR);
    ui_ring(x, y, w, h, ROW_RADIUS, THEME_RING, THEME_RING_SPREAD);
    ui_outline(x, y, w, h, ROW_RADIUS, 2.0f, THEME_ACCENT);
    float ix = x + 2.0f * px, iy = y + 2.0f, iw = w - 4.0f * px, ih = h - 4.0f;
    ui_rounded(ix, iy, iw, ih, ROW_RADIUS - 2.0f, THEME_PANEL_HIGHLIGHT);
    ui_rounded_gradient(ix, iy + 1.0f, iw, ih - 1.0f, ROW_RADIUS - 2.0f, HOT_STOPS, 3);
}

static float now_progress(app *a, const td_row *r)
{
    uint32_t offset = 0;
    const ctw_airing *cur = now_airing(a, &offset);
    int64_t span = r->end_ms - r->start_ms;
    if (cur == NULL || span <= 0) return 0.0f;
    int64_t done = cur->start_ms + (int64_t)offset - r->start_ms;
    if (done <= 0) return 0.0f;
    if (done >= span) return 1.0f;
    return (float)done / (float)span;
}

static void draw_row(app *a, const ctw_channel *ch, size_t i, int now, float y)
{
    const td_row *r = &rows[i];
    asp_rect l = layout.list;
    float px = ui_pixel_x();
    bool past = now >= 0 && (int)i < now;
    bool is_now = (int)i == now;
    if ((int)i == highlight) draw_hot(l.x, y, l.w, TD_ROW_H);
    else if (is_now) ui_rounded(l.x, y, l.w, TD_ROW_H, ROW_RADIUS, THEME_ROW_ACTIVE_FILL);
    if (is_now) {
        ui_rounded(l.x + EDGE_X * px, y + EDGE_Y, EDGE_W * px, TD_ROW_H - 2.0f * EDGE_Y, EDGE_W * px / 2.0f, THEME_ACCENT);
        ui_bar(l.x + BAR_X * px, y + TD_ROW_H - BAR_DY, l.w - 2.0f * BAR_X * px, BAR_H, now_progress(a, r));
    }

    asp_rect row = { l.x, y, l.w, TD_ROW_H };
    today_cols c = today_cols_for(row, time_w, code_w, px);
    row_text *t = &texts[i];
    if (!t->valid) refresh(r, t, &c, l.x + l.w - BAR_X * px);
    uint32_t ink = past ? THEME_TEXT_FADED : THEME_TEXT_PRIMARY;
    uint32_t soft = past ? THEME_TEXT_FADED : THEME_TEXT_SECONDARY;
    ui_text(FONT_BODY, TIME_PX, c.time_x, y + TIME_DY, t->time, soft, ALIGN_LEFT);
    if (r->kind != TD_EPISODE || t->show[0] == '\0') {
        ui_text(FONT_BODY, TITLE_PX, c.show_x, y + TITLE_DY, t->title, soft, ALIGN_LEFT);
        return;
    }
    char path[ADDR_URL_BYTES];
    gt_icon_path(a->root, ch, r->first->show, path, sizeof path);
    icons_draw(path, c.icon_x - (ROW_ICON_PX - c.icon_w) / 2.0f, y + ICON_DY, ROW_ICON_PX);
    ui_text(FONT_BOLD, SHOW_PX, c.show_x, y + SHOW_DY, t->show, ink, ALIGN_LEFT);
    ui_text(FONT_BOLD, CODE_PX, c.code_x, y + CODE_DY, t->code, soft, ALIGN_LEFT);
    ui_text(FONT_BODY, TITLE_PX, c.title_x, y + TITLE_DY, t->title, ink, ALIGN_LEFT);
}

static void draw_rows(app *a, const ctw_channel *ch)
{
    asp_rect l = layout.list;
    int now = now_row(a);
    float cx = l.x - CLIP_SPREAD;
    GRRLIB_ClipDrawing((u32)(cx < 0.0f ? 0.0f : cx), (u32)l.y, (u32)(l.w + 2.0f * CLIP_SPREAD), (u32)l.h);
    for (size_t i = 0; i < row_count; i++) {
        float dy = td_row_y((int)i) - scroll;
        if (dy + TD_ROW_H < 0.0f || dy > l.h) continue;
        draw_row(a, ch, i, now, l.y + dy);
    }
    GRRLIB_ClipReset();
}

static void draw_scrollbar(void)
{
    float content = td_content_h(row_count), view = layout.list.h;
    if (content <= view) return;
    asp_rect s = layout.scroll;
    ui_rounded(s.x, s.y, s.w, s.h, s.w / 2.0f, THEME_BAR_TRACK);
    float th = s.h * view / content;
    if (th < SCROLL_MIN_THUMB) th = SCROLL_MIN_THUMB;
    float ty = s.y + (s.h - th) * (scroll / (content - view));
    ui_rounded(s.x, ty, s.w, th, s.w / 2.0f, THEME_PILL_BORDER);
}

static void draw_header(app *a, const ctw_channel *ch)
{
    char path[ADDR_URL_BYTES];
    if (ch != NULL) {
        gt_icon_path(a->root, ch, NULL, path, sizeof path);
        icons_draw(path, layout.icon.x - (ICON_PX - layout.icon.w) / 2.0f, layout.icon.y, ICON_PX);
    }
    asp_rect n = layout.name;
    ui_text(FONT_BOLD, NAME_PX, n.x, n.y, header_name, THEME_ACCENT, ALIGN_LEFT);
    ui_number_pill(n.x + ui_text_width(FONT_BOLD, NAME_PX, header_name) + NUMBER_GAP, n.y + NUMBER_DY, NUMBER_H, header_number, NUMBER_PX, false);
    ui_text(FONT_BODY, DATE_PX, layout.date.x, layout.date.y, header_date, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    if (looping(a)) {
        asp_rect t = layout.tag;
        float w = ui_text_width(FONT_BOLD, TAG_PX, "Looping") + 2.0f * TAG_PAD * ui_pixel_x();
        ui_rounded(t.x - w, t.y, w, t.h, t.h / 2.0f, THEME_BAR_TRACK);
        ui_text(FONT_BOLD, TAG_PX, t.x - w / 2.0f, t.y + (t.h - (float)TAG_PX) / 2.0f, "Looping", THEME_TEXT_LABEL, ALIGN_CENTER);
    }
    asp_rect r = layout.rule_top;
    GRRLIB_Rectangle(r.x, r.y, r.w, r.h, THEME_BAR_TRACK, 1);
}

static float draw_key(float x, float y, const char *key, const char *label)
{
    float d = layout.hints.h, px = ui_pixel_x(), dw = d * px;
    ui_rounded(x, y, dw, d, d / 2.0f, THEME_PILL_BORDER);
    ui_rounded_gradient(x + px, y + 1.0f, dw - 2.0f * px, d - 2.0f, (d - 2.0f) / 2.0f, DISC_STOPS, 2);
    ui_text(FONT_BOLD, TL_HINT_PX, x + dw / 2.0f, y + (d - (float)TL_HINT_PX) / 2.0f, key, THEME_TEXT_PRIMARY, ALIGN_CENTER);
    float lx = x + dw + KEY_GAP * px;
    ui_text(FONT_BOLD, TL_HINT_PX, lx, y + (d - (float)TL_HINT_PX) / 2.0f, label, THEME_TEXT_LABEL, ALIGN_LEFT);
    return lx + ui_text_width(FONT_BOLD, TL_HINT_PX, label) - x;
}

static void draw_footer(app *a)
{
    asp_rect r = layout.rule_bottom;
    GRRLIB_Rectangle(r.x, r.y, r.w, r.h, THEME_BAR_TRACK, 1);
    float x = layout.hints.x, y = layout.hints.y;
    if (looping(a)) x += draw_key(x, y, "A", "Play from here") + HINT_GAP;
    draw_key(x, y, "B", "Close");
    if (looping(a)) {
        ui_text(FONT_BODY, NOTE_PX, layout.skip_note.x, layout.skip_note.y, "\xE2\x97\x80 \xE2\x96\xB6 skip in fullscreen", THEME_TEXT_SECONDARY, ALIGN_RIGHT);
    }
}

void today_draw(app *a)
{
    int64_t s = tn_shift(&a->tuner, channel);
    if (s != shift) {
        shift = s;
        if (texts != NULL) memset(texts, 0, row_count * sizeof *texts);
    }
    const ctw_channel *ch = ctw_db_channel(a->db, channel);
    GRRLIB_Rectangle(0.0f, 0.0f, 640.0f, 480.0f, THEME_SCRIM, 1);
    asp_rect c = layout.card;
    ui_card(c.x, c.y, c.w, c.h, THEME_PANEL_RADIUS, CARD_MID, THEME_GLOSS_BOTTOM, UI_SHADOW_VIDEO);
    draw_header(a, ch);
    if (ch != NULL) draw_rows(a, ch);
    draw_scrollbar();
    draw_footer(a);
}
