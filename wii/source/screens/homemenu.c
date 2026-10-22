#include "homemenu.h"

#include <stdio.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "../guide_text.h"
#include "../home_layout.h"
#include "../osd_layout.h"
#include "../input.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/glyphs.h"
#include "../ui/press.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define TITLE_PX 24
#define PLAYING_PX 13
#define BIG_PX 20
#define RESUME_PX 15
#define VOL_LABEL_PX 13
#define BATTERY_PX 13
#define BADGE_GLYPH_H 19.6f
#define MENU_GLYPH_H 24.0f
#define MENU_GLYPH_GAP 8.0f
#define BAR_SHADOW 0x0000004Du
#define BAR_SHADOW_DY 2.7f
#define BAR_SHADOW_BLUR 9.0f
#define BADGE_RING_SPREAD 2.0f

static const ui_stop TOP_STOPS[] = { { 0.0f, THEME_PANEL_FILL }, { 0.6f, THEME_PANEL_FILL }, { 1.0f, THEME_GLOSS_COOL } };
static const ui_stop BOTTOM_STOPS[] = { { 0.0f, THEME_PANEL_FILL }, { 0.5f, THEME_PANEL_FILL }, { 1.0f, THEME_GLOSS_DEEP } };

static home_btn hot_btn;
static ps_state press;
static home_layout layout;
static bool laid_out;

static const home_layout *current_layout(void)
{
    if (!laid_out) {
        layout = home_layout_make(ui_pixel_x());
        laid_out = true;
    }
    return &layout;
}

void home_open(app *a)
{
    a->home_open = true;
    hot_btn = HOME_BTN_NONE;
    press.armed = 0;
    sfx_play(SFX_SELECT);
}

static void home_close(app *a)
{
    a->home_open = false;
    hot_btn = HOME_BTN_NONE;
    press.armed = 0;
    sfx_play(SFX_BACK);
}

static void choose(app *a, home_btn btn)
{
    sfx_play(SFX_SELECT);
    switch (btn) {
    case HOME_BTN_WII_MENU:
        app_quit(a, QUIT_DEST_WII_MENU);
    case HOME_BTN_HOMEBREW:
        app_quit(a, QUIT_DEST_HOMEBREW);
    case HOME_BTN_RESUME:
        a->home_open = false;
        hot_btn = HOME_BTN_NONE;
        press.armed = 0;
        break;
    case HOME_BTN_NONE:
        break;
    }
}

void home_update(app *a)
{
    if (!a->home_open) {
        if (in_pressed(WPAD_BUTTON_HOME)) home_open(a);
        return;
    }
    if (in_pressed(WPAD_BUTTON_HOME)) {
        home_close(a);
        return;
    }

    home_btn btn = HOME_BTN_NONE;
    if (cur_raw_visible()) btn = home_hit(current_layout(), cur_x(), cur_y());
    if (btn != HOME_BTN_NONE && btn != hot_btn) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot_btn = btn;
    int fired = ps_update(&press, (int)btn, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired != 0) choose(a, (home_btn)fired);
}

static ui_pill_state state_for(home_btn btn)
{
    if (ps_pressed(&press, (int)hot_btn) == (int)btn && btn != HOME_BTN_NONE) return UI_PILL_PRESSED;
    return hot_btn == btn ? UI_PILL_HOT : UI_PILL_REST;
}

static void top_bar(const home_layout *l)
{
    ui_shadow(l->top.x, l->top.y, l->top.w, l->top.h, 0.0f, BAR_SHADOW, BAR_SHADOW_DY, BAR_SHADOW_BLUR);
    ui_rounded_gradient(l->top.x, l->top.y, l->top.w, l->top.h, 0.0f, TOP_STOPS, 3);
    GRRLIB_Rectangle(l->top.x, l->top.y + l->top.h - 1.0f, l->top.w, 1.0f, THEME_PANEL_BORDER, 1);
}

static void bottom_bar(const home_layout *l)
{
    ui_shadow(l->bottom.x, l->bottom.y, l->bottom.w, l->bottom.h, 0.0f, BAR_SHADOW, -BAR_SHADOW_DY, BAR_SHADOW_BLUR);
    ui_rounded_gradient(l->bottom.x, l->bottom.y, l->bottom.w, l->bottom.h, 0.0f, BOTTOM_STOPS, 3);
    GRRLIB_Rectangle(l->bottom.x, l->bottom.y, l->bottom.w, 1.0f, THEME_PANEL_BORDER, 1);
    GRRLIB_Rectangle(l->bottom.x, l->bottom.y + 1.0f, l->bottom.w, 1.0f, THEME_PANEL_HIGHLIGHT, 1);
}

static void badge(const home_layout *l)
{
    asp_rect b = l->badge;
    ui_ring(b.x, b.y, b.w, b.h, b.h / 2.0f, THEME_RING, BADGE_RING_SPREAD);
    ui_accent_tile(b.x, b.y, b.w, b.h, b.h / 2.0f);
    float gw = BADGE_GLYPH_H * ui_pixel_x();
    ui_glyph_draw(GLYPH_HOME, b.x + (b.w - gw) / 2.0f, b.y + (b.h - BADGE_GLYPH_H) / 2.0f, BADGE_GLYPH_H, THEME_PANEL_FILL);
}

static void playing(app *a, const home_layout *l)
{
    const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
    if (ch == NULL) return;
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, ch->number, a->today, a->ms_of_day, &offset);
    char buf[TF_LINE_BYTES], fitted[TF_LINE_BYTES];
    if (air != NULL && air->kind == CTW_AIR_EPISODE && air->episode != NULL) {
        snprintf(buf, sizeof buf, "%s \xC2\xB7 S%02uE%02u", ch->name, (unsigned)air->episode->season, (unsigned)air->episode->episode);
    } else {
        snprintf(buf, sizeof buf, "%s", ch->name);
    }
    ui_text_fit(FONT_BODY, PLAYING_PX, buf, l->playing.w, fitted, sizeof fitted);
    ui_text(FONT_BODY, PLAYING_PX, l->playing.x + l->playing.w, l->playing.y, fitted, THEME_TEXT_CLOCK, ALIGN_RIGHT);
}

static void wii_menu_pill(const home_layout *l)
{
    asp_rect r = l->wii_menu;
    ui_pill_state state = state_for(HOME_BTN_WII_MENU);
    ui_pill_ex(r.x, r.y, r.w, r.h, NULL, BIG_PX, state, UI_SHADOW_VIDEO);
    float y = r.y + (state == UI_PILL_PRESSED ? 2.0f : 0.0f);
    float gw = MENU_GLYPH_H * ui_pixel_x();
    float tw = ui_text_width(FONT_BOLD, BIG_PX, "Wii Menu");
    float gx = r.x + (r.w - (gw + MENU_GLYPH_GAP + tw)) / 2.0f;
    ui_glyph_draw(GLYPH_HOME, gx, y + (r.h - MENU_GLYPH_H) / 2.0f, MENU_GLYPH_H, THEME_ACCENT);
    ui_text(FONT_BOLD, BIG_PX, gx + gw + MENU_GLYPH_GAP, y + r.h / 2.0f - (float)BIG_PX / 2.0f, "Wii Menu", THEME_TEXT_PRIMARY, ALIGN_LEFT);
}

void home_draw(app *a)
{
    if (!a->home_open) return;
    GRRLIB_Rectangle(0, 0, 640, 480, RGBA(0, 0, 0, HOME_DIM_ALPHA), 1);

    const home_layout *l = current_layout();
    top_bar(l);
    badge(l);
    ui_text(FONT_BOLD, TITLE_PX, l->title.x, l->title.y, "HOME Menu", THEME_TEXT_PRIMARY, ALIGN_LEFT);
    playing(a, l);

    wii_menu_pill(l);
    ui_pill_ex(l->hbc.x, l->hbc.y, l->hbc.w, l->hbc.h, "Back to Homebrew Channel", BIG_PX, state_for(HOME_BTN_HOMEBREW), UI_SHADOW_VIDEO);
    ui_pill_ex(l->resume.x, l->resume.y, l->resume.w, l->resume.h, "Resume", RESUME_PX, state_for(HOME_BTN_RESUME), UI_SHADOW_VIDEO);

    bottom_bar(l);
    ui_text(FONT_BOLD, VOL_LABEL_PX, l->vol_label.x, l->vol_label.y, "Volume", THEME_TEXT_LABEL, ALIGN_LEFT);
    ui_segments(l->vol_bar.x, l->vol_bar.y, l->vol_bar.w, l->vol_bar.h, HOME_SEGMENTS, HOME_SEGMENT_GAP, osd_segments_lit(a->volume), false);
    ui_battery(l->battery_label.x, l->battery.y, l->battery.h, home_battery_bars(in_battery_level()), BATTERY_PX, NULL);
}
