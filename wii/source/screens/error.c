#include "error.h"

#include <wiiuse/wpad.h>

#include "../input.h"
#include "../ui/draw.h"
#include "../ui/press.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/cursor.h"
#include "../ui/theme.h"

#define BODY_MAX_LINES 14
#define PILL_HBC 1
#define PILL_SETTINGS 2

static const float SCREEN_W = 640.0f, SCREEN_H = 480.0f;
static const float PANEL_W = 440.0f, PAD = 24.0f;
static const float TITLE_H = 28.0f, TITLE_GAP = 12.0f, BODY_LINE_H = 20.0f, PILL_GAP = 20.0f;
static const float HBC_W = 240.0f, SETTINGS_W = 140.0f, PILL_H = 40.0f;
static const int TITLE_PX = 20, BODY_PX = 15, PILL_PX = 14;

static char title_line[TF_LINE_BYTES];
static tf_line body_lines[BODY_MAX_LINES];
static size_t body_count;
static float panel_x, panel_y, panel_h;
static ps_state press;
static int hot;

static float hbc_x(void)
{
    return panel_x + PAD;
}

static float settings_x(void)
{
    return panel_x + PANEL_W - PAD - SETTINGS_W;
}

static float pill_y(void)
{
    return panel_y + panel_h - PILL_H - PAD;
}

void error_enter(app *a)
{
    float text_w = PANEL_W - 2.0f * PAD;
    ui_text_fit(FONT_BOLD, TITLE_PX, a->error_title, text_w, title_line, sizeof title_line);
    body_count = ui_text_wrap(FONT_BODY, BODY_PX, a->error_body, text_w, BODY_MAX_LINES, body_lines);
    panel_h = PAD + TITLE_H + TITLE_GAP + (float)body_count * BODY_LINE_H + PILL_GAP + PILL_H + PAD;
    panel_x = (SCREEN_W - PANEL_W) / 2.0f;
    panel_y = (SCREEN_H - panel_h) / 2.0f;
    press.armed = 0;
    hot = 0;
}

static bool point_in(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

static int pill_at(float px, float py)
{
    if (point_in(px, py, hbc_x(), pill_y(), HBC_W, PILL_H)) return PILL_HBC;
    if (point_in(px, py, settings_x(), pill_y(), SETTINGS_W, PILL_H)) return PILL_SETTINGS;
    return 0;
}

void error_update(app *a)
{
    int now_hot = cur_visible() ? pill_at(cur_x(), cur_y()) : 0;
    if (now_hot != 0 && now_hot != hot) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot = now_hot;
    int fired = ps_update(&press, hot, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired == PILL_HBC) {
        sfx_play(SFX_SELECT);
        app_quit(a, QUIT_DEST_HOMEBREW);
    }
    if (fired == PILL_SETTINGS) {
        sfx_play(SFX_SELECT);
        app_open_settings(a);
    }
}

static ui_pill_state state_for(int pill)
{
    if (ps_pressed(&press, hot) == pill) return UI_PILL_PRESSED;
    return hot == pill ? UI_PILL_HOT : UI_PILL_REST;
}

void error_draw(app *a)
{
    (void)a;
    ui_stripes();
    ui_card(panel_x, panel_y, PANEL_W, panel_h, THEME_PANEL_RADIUS, 0.6f, THEME_GLOSS_BOTTOM, UI_SHADOW_PANEL);
    ui_text(FONT_BOLD, TITLE_PX, panel_x + PAD, panel_y + PAD, title_line, THEME_TEXT_PRIMARY, ALIGN_LEFT);

    float y = panel_y + PAD + TITLE_H + TITLE_GAP;
    for (size_t i = 0; i < body_count; i++) {
        ui_text(FONT_BODY, BODY_PX, panel_x + PAD, y, body_lines[i].text, THEME_TEXT_SECONDARY, ALIGN_LEFT);
        y += BODY_LINE_H;
    }

    ui_pill_ex(hbc_x(), pill_y(), HBC_W, PILL_H, "Back to Homebrew Channel", PILL_PX, state_for(PILL_HBC), UI_SHADOW_PANEL);
    ui_pill_ex(settings_x(), pill_y(), SETTINGS_W, PILL_H, "Settings", PILL_PX, state_for(PILL_SETTINGS), UI_SHADOW_PANEL);
}
