#include "error.h"

#include <wiiuse/wpad.h>

#include "../input.h"
#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/cursor.h"
#include "../ui/theme.h"

#define BODY_MAX_LINES 14

static const float SCREEN_W = 640.0f, SCREEN_H = 480.0f;
static const float PANEL_W = 440.0f, PAD = 24.0f;
static const float TITLE_H = 28.0f, TITLE_GAP = 12.0f, BODY_LINE_H = 20.0f, PILL_GAP = 20.0f;
static const float PILL_W = 260.0f, PILL_H = 40.0f;
static const int TITLE_PX = 20, BODY_PX = 15;

static char title_line[TF_LINE_BYTES];
static tf_line body_lines[BODY_MAX_LINES];
static size_t body_count;
static float panel_x, panel_y, panel_h;

static float pill_x(void)
{
    return panel_x + (PANEL_W - PILL_W) / 2.0f;
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
}

static bool point_in(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

void error_update(app *a)
{
    bool clicked = cur_visible() && in_pressed(WPAD_BUTTON_A) && point_in(cur_x(), cur_y(), pill_x(), pill_y(), PILL_W, PILL_H);
    if (clicked || in_pressed(WPAD_BUTTON_HOME)) app_quit(a);
}

void error_draw(app *a)
{
    (void)a;
    ui_stripes();
    ui_panel(panel_x, panel_y, PANEL_W, panel_h, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);
    ui_text(FONT_BOLD, TITLE_PX, panel_x + PAD, panel_y + PAD, title_line, THEME_TEXT_PRIMARY, ALIGN_LEFT);

    float y = panel_y + PAD + TITLE_H + TITLE_GAP;
    for (size_t i = 0; i < body_count; i++) {
        ui_text(FONT_BODY, BODY_PX, panel_x + PAD, y, body_lines[i].text, THEME_TEXT_SECONDARY, ALIGN_LEFT);
        y += BODY_LINE_H;
    }

    bool hot = cur_visible() && point_in(cur_x(), cur_y(), pill_x(), pill_y(), PILL_W, PILL_H);
    ui_pill(pill_x(), pill_y(), PILL_W, PILL_H, "Back to Homebrew Channel", hot);
}
