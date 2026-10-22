#include "header.h"

#include <grrlib.h>

#include "../home_layout.h"
#include "../input.h"
#include "../ui/draw.h"
#include "../ui/theme.h"
#include "logo.h"

#define SCREEN_W 640.0f
#define SHADOW_COLOR 0x344A6014u
#define SHADOW_DY 2.0f
#define SHADOW_BLUR 6.0f
#define BATTERY_X 567.6f
#define BATTERY_Y 12.7f
#define BATTERY_PILL_H 22.0f
#define BATTERY_LABEL_PX 12
#define BLOOM_CX 320.0f
#define BLOOM_CY 144.0f
#define BLOOM_RX 448.0f
#define BLOOM_RY 336.0f
#define BLOOM_COLOR 0xFFFFFFD9u

static const ui_stop HEADER_STOPS[] = { { 0.0f, THEME_PANEL_FILL }, { 0.55f, THEME_PANEL_FILL }, { 1.0f, THEME_GLOSS_SOFT } };

void header_bloom(void)
{
    ui_radial(BLOOM_CX, BLOOM_CY, BLOOM_RX, BLOOM_RY, BLOOM_COLOR);
}

void header_draw(app *a, bool with_logo, float dy)
{
    (void)a;
    ui_shadow(0.0f, dy, SCREEN_W, THEME_HEADER_H, 0.0f, SHADOW_COLOR, SHADOW_DY, SHADOW_BLUR);
    ui_rounded_gradient(0.0f, dy, SCREEN_W, THEME_HEADER_H, 0.0f, HEADER_STOPS, 3);
    GRRLIB_Rectangle(0.0f, dy + THEME_HEADER_H - 1.0f, SCREEN_W, 1.0f, THEME_PANEL_BORDER, 1);
    if (with_logo) logo_draw_header();
    ui_battery(BATTERY_X, BATTERY_Y + dy, BATTERY_PILL_H, home_battery_bars(in_battery_level()), BATTERY_LABEL_PX, NULL);
}
