#include "sysbar_layout.h"

#include "ui/theme.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define CLOCK_W 133.3f
#define CLOCK_Y 420.0f
#define CLOCK_H 28.0f
#define DATE_Y 453.0f
#define DATE_H 15.0f
#define DISC_X 21.3f
#define DISC_Y 437.3f
#define DISC_H 37.0f
#define LABEL_GAP 6.3f
#define LABEL_Y 449.0f
#define LABEL_W 90.0f
#define LABEL_H 15.0f
#define PILL_W 58.0f
#define PILL_H 32.0f
#define PILL_Y 440.9f
#define PREV_X 496.7f
#define NEXT_X 560.7f

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

sysbar_layout sysbar_layout_for(float pixel_x)
{
    sysbar_layout l;
    float body_y = SCREEN_H - THEME_SYSBAR_H;
    l.body = rect(0.0f, body_y, SCREEN_W, THEME_SYSBAR_H);
    l.hump = rect((SCREEN_W - THEME_SYSBAR_HUMP_TOP_W) / 2.0f, body_y - THEME_SYSBAR_HUMP, THEME_SYSBAR_HUMP_TOP_W, THEME_SYSBAR_HUMP);
    l.slope_w = THEME_SYSBAR_HUMP_SLOPE_W;
    l.clock = rect((SCREEN_W - CLOCK_W) / 2.0f, CLOCK_Y, CLOCK_W, CLOCK_H);
    l.date = rect((SCREEN_W - CLOCK_W) / 2.0f, DATE_Y, CLOCK_W, DATE_H);
    l.home_disc = rect(DISC_X, DISC_Y, DISC_H * pixel_x, DISC_H);
    l.home_label = rect(l.home_disc.x + l.home_disc.w + LABEL_GAP, LABEL_Y, LABEL_W, LABEL_H);
    l.home_hit = rect(l.home_disc.x, l.home_disc.y, l.home_label.x + l.home_label.w - l.home_disc.x, DISC_H);
    l.prev = rect(PREV_X, PILL_Y, PILL_W, PILL_H);
    l.next = rect(NEXT_X, PILL_Y, PILL_W, PILL_H);
    return l;
}

static asp_rect moved(asp_rect r, float dy)
{
    r.y += dy;
    return r;
}

sysbar_layout sysbar_layout_shift(const sysbar_layout *l, float dy)
{
    sysbar_layout s = *l;
    s.body = moved(l->body, dy);
    s.hump = moved(l->hump, dy);
    s.clock = moved(l->clock, dy);
    s.date = moved(l->date, dy);
    s.home_disc = moved(l->home_disc, dy);
    s.home_label = moved(l->home_label, dy);
    s.home_hit = moved(l->home_hit, dy);
    s.prev = moved(l->prev, dy);
    s.next = moved(l->next, dy);
    return s;
}

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

sysbar_btn sysbar_hit(const sysbar_layout *l, float x, float y, bool channels)
{
    if (inside(l->home_hit, x, y)) return SYSBAR_BTN_HOME;
    if (channels && inside(l->prev, x, y)) return SYSBAR_BTN_PREV;
    if (channels && inside(l->next, x, y)) return SYSBAR_BTN_NEXT;
    return SYSBAR_BTN_NONE;
}

static float ease(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

float sysbar_top_at(const sysbar_layout *l, float x)
{
    float low = l->body.y, high = l->hump.y, rise = low - high;
    float left_start = l->hump.x - l->slope_w, right_end = l->hump.x + l->hump.w + l->slope_w;
    if (x <= left_start || x >= right_end) return low;
    if (x < l->hump.x) return low - rise * ease((x - left_start) / l->slope_w);
    if (x <= l->hump.x + l->hump.w) return high;
    return high + rise * ease((x - l->hump.x - l->hump.w) / l->slope_w);
}
