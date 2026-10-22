#include "home_layout.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define BAR_H 72.0f
#define BIG_W 340.0f
#define BIG_H 60.0f
#define BIG_GAP 28.0f
#define RESUME_W 190.0f
#define RESUME_H 40.0f
#define BATTERY_X 520.0f
#define BATTERY_BAR_W 12.0f
#define BATTERY_GAP 5.0f
#define BATTERY_BAR_MIN_H 10.0f
#define BATTERY_BAR_STEP 6.0f

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

home_layout home_layout_make(void)
{
    home_layout l;
    l.top = rect(0.0f, 0.0f, SCREEN_W, BAR_H);
    l.bottom = rect(0.0f, SCREEN_H - BAR_H, SCREEN_W, BAR_H);
    l.title = rect(0.0f, 20.0f, SCREEN_W, 32.0f);

    float bx = (SCREEN_W - BIG_W) / 2.0f;
    float by = (SCREEN_H - (2.0f * BIG_H + BIG_GAP)) / 2.0f;
    l.wii_menu = rect(bx, by, BIG_W, BIG_H);
    l.hbc = rect(bx, by + BIG_H + BIG_GAP, BIG_W, BIG_H);

    float ry = l.bottom.y + (BAR_H - RESUME_H) / 2.0f;
    l.resume = rect((SCREEN_W - RESUME_W) / 2.0f, ry, RESUME_W, RESUME_H);

    float base = l.bottom.y + (BAR_H + BATTERY_BAR_MIN_H + BATTERY_BAR_STEP * (float)(HOME_BATTERY_BARS - 1)) / 2.0f;
    for (int i = 0; i < HOME_BATTERY_BARS; i++) {
        float h = BATTERY_BAR_MIN_H + BATTERY_BAR_STEP * (float)i;
        l.battery[i] = rect(BATTERY_X + (BATTERY_BAR_W + BATTERY_GAP) * (float)i, base - h, BATTERY_BAR_W, h);
    }
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
