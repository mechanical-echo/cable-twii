#include "error.h"

#include <stdlib.h>
#include <wiiuse/wpad.h>

#include "../input.h"
#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/cursor.h"
#include "../ui/theme.h"

static const float PANEL_X = 120.0f, PANEL_Y = 140.0f, PANEL_W = 400.0f, PANEL_H = 200.0f;
static const float PILL_W = 260.0f, PILL_H = 40.0f;

void error_enter(app *a)
{
    (void)a;
}

static bool point_in(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

void error_update(app *a)
{
    float pill_x = PANEL_X + (PANEL_W - PILL_W) / 2.0f;
    float pill_y = PANEL_Y + PANEL_H - PILL_H - 24.0f;

    bool clicked = cur_visible() && in_pressed(WPAD_BUTTON_A) && point_in(cur_x(), cur_y(), pill_x, pill_y, PILL_W, PILL_H);
    if (clicked || in_pressed(WPAD_BUTTON_HOME)) {
        (void)a;
        exit(0);
    }
}

void error_draw(app *a)
{
    ui_stripes();
    ui_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);
    ui_text(FONT_BOLD, 20, PANEL_X + 24, PANEL_Y + 24, a->error_title, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, 15, PANEL_X + 24, PANEL_Y + 64, a->error_body, THEME_TEXT_SECONDARY, ALIGN_LEFT);

    float pill_x = PANEL_X + (PANEL_W - PILL_W) / 2.0f;
    float pill_y = PANEL_Y + PANEL_H - PILL_H - 24.0f;
    bool hot = cur_visible() && point_in(cur_x(), cur_y(), pill_x, pill_y, PILL_W, PILL_H);
    ui_pill(pill_x, pill_y, PILL_W, PILL_H, "Back to Homebrew Channel", hot);
}
