#include "home_layout.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define BAR_H 72.0f
#define BADGE_X 26.0f
#define BADGE_Y 16.9f
#define BADGE_H 37.0f
#define TITLE_GAP 8.0f
#define TITLE_Y 22.4f
#define TITLE_H 24.0f
#define PLAYING_RIGHT 614.0f
#define PLAYING_Y 27.6f
#define PLAYING_W 240.0f
#define PLAYING_H 13.3f
#define BIG_X 150.0f
#define BIG_W 340.0f
#define BIG_H 60.0f
#define WII_MENU_Y 133.3f
#define HBC_Y 221.3f
#define RESUME_X 225.0f
#define RESUME_Y 311.1f
#define RESUME_W 190.0f
#define RESUME_H 40.0f
#define VOL_LABEL_X 26.0f
#define VOL_LABEL_Y 436.4f
#define VOL_LABEL_W 36.0f
#define VOL_LABEL_H 16.0f
#define VOL_BAR_X 68.2f
#define VOL_BAR_H 16.0f
#define BATTERY_LABEL_X 554.1f
#define BATTERY_LABEL_W 13.0f
#define BATTERY_X 572.3f
#define BATTERY_Y 431.1f
#define BATTERY_W 41.7f
#define BATTERY_H 26.7f

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

home_layout home_layout_make(float pixel_x)
{
    home_layout l;
    l.top = rect(0.0f, 0.0f, SCREEN_W, BAR_H);
    l.bottom = rect(0.0f, SCREEN_H - BAR_H, SCREEN_W, BAR_H);
    l.badge = rect(BADGE_X, BADGE_Y, BADGE_H * pixel_x, BADGE_H);
    float title_x = l.badge.x + l.badge.w + TITLE_GAP;
    l.title = rect(title_x, TITLE_Y, PLAYING_RIGHT - PLAYING_W - TITLE_GAP - title_x, TITLE_H);
    l.playing = rect(PLAYING_RIGHT - PLAYING_W, PLAYING_Y, PLAYING_W, PLAYING_H);
    l.wii_menu = rect(BIG_X, WII_MENU_Y, BIG_W, BIG_H);
    l.hbc = rect(BIG_X, HBC_Y, BIG_W, BIG_H);
    l.resume = rect(RESUME_X, RESUME_Y, RESUME_W, RESUME_H);
    l.vol_label = rect(VOL_LABEL_X, VOL_LABEL_Y, VOL_LABEL_W, VOL_LABEL_H);
    float bar_w = HOME_SEGMENT_W * (float)HOME_SEGMENTS + HOME_SEGMENT_GAP * (float)(HOME_SEGMENTS - 1);
    l.vol_bar = rect(VOL_BAR_X, VOL_LABEL_Y, bar_w, VOL_BAR_H);
    l.battery_label = rect(BATTERY_LABEL_X, VOL_LABEL_Y, BATTERY_LABEL_W, VOL_LABEL_H);
    l.battery = rect(BATTERY_X, BATTERY_Y, BATTERY_W, BATTERY_H);
    return l;
}

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

home_btn home_hit(const home_layout *l, float x, float y)
{
    if (inside(l->wii_menu, x, y)) return HOME_BTN_WII_MENU;
    if (inside(l->hbc, x, y)) return HOME_BTN_HOMEBREW;
    if (inside(l->resume, x, y)) return HOME_BTN_RESUME;
    return HOME_BTN_NONE;
}

int home_battery_bars(uint8_t level)
{
    return ((int)level * HOME_BATTERY_BARS + 254) / 255;
}
