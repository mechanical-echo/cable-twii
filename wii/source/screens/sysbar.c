#include "sysbar.h"

#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "../guide_text.h"
#include "../input.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/glyphs.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define SLICE_PX 2.0f
#define MAX_POINTS 128
#define SHADOW_BAND_PX 5.0f
#define SHADOW_BAND_COLOR 0x344A6012u
#define OUTLINE_PX 1.0f
#define CLOCK_PX 28
#define DATE_PX 12
#define LABEL_PX 12
#define PILL_PX 13
#define GLYPH_H 19.6f
#define DISC_HOT_BORDER 3.0f
#define DISC_REST_BORDER 2.0f

static const ui_stop BAR_STOPS[] = { { 0.0f, THEME_PANEL_FILL }, { 0.5f, THEME_GLOSS_BOTTOM }, { 1.0f, THEME_GLOSS_DEEP } };
static sysbar_btn hot_btn;

sysbar_btn sysbar_update(app *a, const sysbar_layout *l, bool channels, ps_state *ps)
{
    (void)a;
    sysbar_btn hot = cur_visible() ? sysbar_hit(l, cur_x(), cur_y(), channels) : SYSBAR_BTN_NONE;
    if (hot != SYSBAR_BTN_NONE && hot != hot_btn) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot_btn = hot;
    int fired = ps_update(ps, (int)hot, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired != 0) sfx_play(SFX_SELECT);
    return (sysbar_btn)fired;
}

static int curve_points(const sysbar_layout *l, float *xs, float *ys)
{
    int n = 0;
    float left = l->hump.x - l->slope_w, right = l->hump.x + l->hump.w + l->slope_w;
    xs[n] = 0.0f;
    ys[n++] = l->body.y;
    for (float x = left; x <= right + 0.01f && n < MAX_POINTS - 1; x += SLICE_PX) {
        xs[n] = x;
        ys[n++] = sysbar_top_at(l, x);
    }
    xs[n] = SCREEN_W;
    ys[n++] = l->body.y;
    return n;
}

static void fill_slice(float x0, float x1, float top0, float top1, float gy, float gh)
{
    float mid = gy + gh * BAR_STOPS[1].pos;
    float xs[4], ys[4];
    uint32_t cs[4];
    float t0 = top0 < mid ? top0 : mid, t1 = top1 < mid ? top1 : mid;
    xs[0] = x0; ys[0] = t0; cs[0] = ui_grad_at(BAR_STOPS, 3, (t0 - gy) / gh);
    xs[1] = x1; ys[1] = t1; cs[1] = ui_grad_at(BAR_STOPS, 3, (t1 - gy) / gh);
    xs[2] = x1; ys[2] = mid; cs[2] = BAR_STOPS[1].color;
    xs[3] = x0; ys[3] = mid; cs[3] = BAR_STOPS[1].color;
    ui_quad(xs, ys, cs);
    xs[0] = x0; ys[0] = mid; cs[0] = BAR_STOPS[1].color;
    xs[1] = x1; ys[1] = mid; cs[1] = BAR_STOPS[1].color;
    xs[2] = x1; ys[2] = SCREEN_H; cs[2] = BAR_STOPS[2].color;
    xs[3] = x0; ys[3] = SCREEN_H; cs[3] = BAR_STOPS[2].color;
    ui_quad(xs, ys, cs);
}

static void draw_bar(const sysbar_layout *l)
{
    float xs[MAX_POINTS], ys[MAX_POINTS];
    int n = curve_points(l, xs, ys);
    float gy = l->hump.y, gh = SCREEN_H - gy;
    for (int i = 0; i + 1 < n; i++) fill_slice(xs[i], xs[i + 1], ys[i], ys[i + 1], gy, gh);
    float band[MAX_POINTS];
    for (int i = 0; i < n; i++) band[i] = ys[i] - SHADOW_BAND_PX / 2.0f;
    ui_polyline(xs, band, n, SHADOW_BAND_PX, SHADOW_BAND_COLOR);
    for (int i = 0; i < n; i++) band[i] = ys[i] + OUTLINE_PX / 2.0f;
    ui_polyline(xs, band, n, OUTLINE_PX, THEME_PANEL_BORDER);
    for (int i = 0; i < n; i++) band[i] = ys[i] + OUTLINE_PX * 1.5f;
    ui_polyline(xs, band, n, OUTLINE_PX, THEME_PANEL_HIGHLIGHT);
}

static void draw_home(const sysbar_layout *l, bool hot, bool pressed)
{
    asp_rect d = l->home_disc;
    float y = d.y + (pressed ? 2.0f : 0.0f);
    float r = d.h / 2.0f;
    if (!pressed) ui_drop_shadow(d.x, y, d.w, d.h, r, UI_SHADOW_PANEL, 1.5f);
    if (hot && !pressed) ui_ring(d.x, y, d.w, d.h, r, THEME_RING_STRONG, THEME_RING_SPREAD);
    if (pressed) ui_ring(d.x, y, d.w, d.h, r, THEME_RING, THEME_RING_SPREAD);
    ui_disc(d.x, y, d.h, THEME_PANEL_FILL, THEME_DISC_MID, THEME_DISC_EDGE);
    ui_outline(d.x, y, d.w, d.h, r, hot ? DISC_HOT_BORDER : DISC_REST_BORDER, hot ? THEME_ACCENT : THEME_PILL_BORDER);
    float gw = GLYPH_H * ui_pixel_x();
    ui_glyph_draw(GLYPH_HOME, d.x + (d.w - gw) / 2.0f, y + (d.h - GLYPH_H) / 2.0f, GLYPH_H, THEME_ACCENT);
    ui_text(FONT_BOLD, LABEL_PX, l->home_label.x, l->home_label.y + (pressed ? 2.0f : 0.0f), "HOME Menu", THEME_TEXT_LABEL, ALIGN_LEFT);
}

static ui_pill_state state_for(sysbar_btn btn, const ps_state *ps)
{
    if (ps_pressed(ps, (int)hot_btn) == (int)btn && btn != SYSBAR_BTN_NONE) return UI_PILL_PRESSED;
    return hot_btn == btn ? UI_PILL_HOT : UI_PILL_REST;
}

void sysbar_draw(app *a, const sysbar_layout *l, bool channels, const ps_state *ps)
{
    draw_bar(l);
    char clock[8], date[16];
    gt_hhmm(a->ms_of_day, clock, sizeof clock);
    gt_date(a->today, date, sizeof date);
    ui_text(FONT_BOLD, CLOCK_PX, l->clock.x + l->clock.w / 2.0f, l->clock.y, clock, THEME_TEXT_CLOCK, ALIGN_CENTER);
    ui_text(FONT_BODY, DATE_PX, l->date.x + l->date.w / 2.0f, l->date.y, date, THEME_TEXT_CLOCK, ALIGN_CENTER);
    ui_pill_state home = state_for(SYSBAR_BTN_HOME, ps);
    draw_home(l, home != UI_PILL_REST, home == UI_PILL_PRESSED);
    if (!channels) return;
    ui_pill_ex(l->prev.x, l->prev.y, l->prev.w, l->prev.h, "\xE2\x97\x80 CH", PILL_PX, state_for(SYSBAR_BTN_PREV, ps), UI_SHADOW_PANEL);
    ui_pill_ex(l->next.x, l->next.y, l->next.w, l->next.h, "CH \xE2\x96\xB6", PILL_PX, state_for(SYSBAR_BTN_NEXT, ps), UI_SHADOW_PANEL);
}
