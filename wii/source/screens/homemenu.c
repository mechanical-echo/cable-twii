#include "homemenu.h"

#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "../home_layout.h"
#include "../input.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define TITLE_PX 24
#define BAR_EDGE_H 2.0f

static home_btn hot_btn;

void home_open(app *a)
{
    a->home_open = true;
    hot_btn = HOME_BTN_NONE;
    sfx_play(SFX_SELECT);
}

static void home_close(app *a)
{
    a->home_open = false;
    hot_btn = HOME_BTN_NONE;
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
    if (cur_raw_visible()) {
        home_layout l = home_layout_make();
        btn = home_hit(&l, cur_x(), cur_y());
    }
    if (btn != HOME_BTN_NONE && btn != hot_btn) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot_btn = btn;
    if (btn != HOME_BTN_NONE && in_pressed(WPAD_BUTTON_A)) choose(a, btn);
}

static void bar(asp_rect r, bool top)
{
    GRRLIB_Rectangle(r.x, r.y, r.w, r.h, THEME_PANEL_FILL, 1);
    float edge_y = top ? r.y + r.h - BAR_EDGE_H : r.y;
    GRRLIB_Rectangle(r.x, edge_y, r.w, BAR_EDGE_H, THEME_PANEL_BORDER, 1);
}

static void battery(const home_layout *l)
{
    int lit = home_battery_bars(in_battery_level());
    for (int i = 0; i < HOME_BATTERY_BARS; i++) {
        asp_rect r = l->battery[i];
        ui_rounded(r.x, r.y, r.w, r.h, 3.0f, i < lit ? THEME_ACCENT : THEME_BAR_TRACK);
    }
}

void home_draw(app *a)
{
    if (!a->home_open) return;
    GRRLIB_Rectangle(0, 0, 640, 480, RGBA(0, 0, 0, HOME_DIM_ALPHA), 1);

    home_layout l = home_layout_make();
    bar(l.top, true);
    bar(l.bottom, false);
    ui_text(FONT_BOLD, TITLE_PX, l.title.x + l.title.w / 2.0f, l.title.y, "HOME Menu", THEME_TEXT_PRIMARY, ALIGN_CENTER);
    ui_pill(l.wii_menu.x, l.wii_menu.y, l.wii_menu.w, l.wii_menu.h, "Wii Menu", hot_btn == HOME_BTN_WII_MENU);
    ui_pill(l.hbc.x, l.hbc.y, l.hbc.w, l.hbc.h, "Homebrew Channel", hot_btn == HOME_BTN_HOMEBREW);
    ui_pill(l.resume.x, l.resume.y, l.resume.w, l.resume.h, "Resume", hot_btn == HOME_BTN_RESUME);
    battery(&l);
}
