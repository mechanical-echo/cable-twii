#include "empty.h"

#include <wiiuse/wpad.h>

#include "../input.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/glyphs.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define CARD_X 153.3f
#define CARD_Y 88.9f
#define CARD_W 333.3f
#define CARD_H 275.6f
#define TILE_Y 114.7f
#define TILE_H 80.0f
#define TILE_RADIUS 18.0f
#define TILE_DASH 4.0f
#define TILE_STROKE 1.5f
#define PLAY_H 37.0f
#define TITLE_PX 24
#define TITLE_Y 209.0f
#define BODY_PX 13
#define BODY_Y 250.0f
#define BODY_LINE_H 18.6f
#define BODY_MAX_LINES 3
#define BODY_INSET 19.0f
#define HBC_X 172.0f
#define HBC_W 206.0f
#define SETTINGS_X 386.0f
#define SETTINGS_W 82.0f
#define PILL_Y 299.4f
#define PILL_H 39.2f
#define PILL_PX 14
#define PILL_HBC 1
#define PILL_SETTINGS 2

static const char BODY[] = "Add one on the Channels page in cable-twii Studio, then plug the drive back in.";
static tf_line body_lines[BODY_MAX_LINES];
static size_t body_count;
static bool wrapped;
static int hot;

static bool in_pill(float px, float py, float x, float w)
{
    return px >= x && px <= x + w && py >= PILL_Y && py <= PILL_Y + PILL_H;
}

static int pill_at(float px, float py)
{
    if (in_pill(px, py, HBC_X, HBC_W)) return PILL_HBC;
    if (in_pill(px, py, SETTINGS_X, SETTINGS_W)) return PILL_SETTINGS;
    return 0;
}

void empty_update(app *a, ps_state *ps)
{
    int now_hot = cur_visible() ? pill_at(cur_x(), cur_y()) : 0;
    if (now_hot != 0 && now_hot != hot) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot = now_hot;
    int fired = ps_update(ps, hot, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired == PILL_HBC) {
        sfx_play(SFX_SELECT);
        app_quit(a, QUIT_DEST_HOMEBREW);
    }
    if (fired == PILL_SETTINGS) {
        sfx_play(SFX_SELECT);
        app_open_settings(a);
    }
}

static ui_pill_state state_for(const ps_state *ps, int pill)
{
    if (ps_pressed(ps, hot) == pill) return UI_PILL_PRESSED;
    return hot == pill ? UI_PILL_HOT : UI_PILL_REST;
}

void empty_draw(app *a, const ps_state *ps)
{
    (void)a;
    if (!wrapped) {
        body_count = ui_text_wrap(FONT_BODY, BODY_PX, BODY, CARD_W - 2.0f * BODY_INSET, BODY_MAX_LINES, body_lines);
        wrapped = true;
    }
    ui_card(CARD_X, CARD_Y, CARD_W, CARD_H, THEME_PANEL_RADIUS, 0.6f, THEME_GLOSS_BOTTOM, UI_SHADOW_PANEL);
    float cx = CARD_X + CARD_W / 2.0f;
    float tile_w = TILE_H * ui_pixel_x();
    float tile_x = cx - tile_w / 2.0f;
    ui_rounded(tile_x, TILE_Y, tile_w, TILE_H, TILE_RADIUS, THEME_PLACEHOLDER_FILL);
    ui_dashed_rounded(tile_x, TILE_Y, tile_w, TILE_H, TILE_RADIUS, TILE_STROKE, TILE_DASH, TILE_DASH, THEME_PILL_BORDER);
    float play_w = PLAY_H * ui_pixel_x();
    ui_glyph_draw(GLYPH_PLAY, cx - play_w / 2.0f, TILE_Y + (TILE_H - PLAY_H) / 2.0f, PLAY_H, THEME_PILL_BORDER);
    ui_text(FONT_BOLD, TITLE_PX, cx, TITLE_Y, "No channels yet", THEME_TEXT_PRIMARY, ALIGN_CENTER);
    for (size_t i = 0; i < body_count; i++) {
        ui_text(FONT_BODY, BODY_PX, cx, BODY_Y + BODY_LINE_H * (float)i, body_lines[i].text, THEME_TEXT_LABEL, ALIGN_CENTER);
    }
    ui_pill_ex(HBC_X, PILL_Y, HBC_W, PILL_H, "Back to Homebrew Channel", PILL_PX, state_for(ps, PILL_HBC), UI_SHADOW_PANEL);
    ui_pill_ex(SETTINGS_X, PILL_Y, SETTINGS_W, PILL_H, "Settings", PILL_PX, state_for(ps, PILL_SETTINGS), UI_SHADOW_PANEL);
}
