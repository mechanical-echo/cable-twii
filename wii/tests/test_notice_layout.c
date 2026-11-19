#include "greatest/greatest.h"
#include "logoanim.h"
#include "notice_layout.h"
#include "ui/theme.h"

#define IMG_W 256.0f
#define IMG_H 64.0f

static bool overlaps(asp_rect a, asp_rect b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static asp_rect logo_rect(void)
{
    asp_rect r;
    logo_header_rect_for(IMG_W, IMG_H, &r.x, &r.y, &r.w, &r.h);
    return r;
}

static asp_rect battery_rect(void)
{
    return (asp_rect){ THEME_HEADER_BATTERY_X, 0.0f, 640.0f - THEME_HEADER_BATTERY_X, THEME_HEADER_H };
}

TEST notice_clears_logo_and_battery(void)
{
    const float pixel_xs[] = { 1.0f, 0.75f };
    const float widths[] = { 90.0f, 200.0f, 330.0f, 420.0f, 520.0f, 600.0f };
    for (size_t i = 0; i < sizeof pixel_xs / sizeof pixel_xs[0]; i++) {
        for (size_t j = 0; j < sizeof widths / sizeof widths[0]; j++) {
            asp_rect r = notice_pill_rect(widths[j], logo_rect(), pixel_xs[i]);
            ASSERT_FALSE(overlaps(r, logo_rect()));
            ASSERT_FALSE(overlaps(r, battery_rect()));
            ASSERT(r.x >= 0.0f && r.x + r.w <= 640.0f);
        }
    }
    PASS();
}

TEST notice_short_stays_centred_in_header(void)
{
    asp_rect r = notice_pill_rect(200.0f, logo_rect(), 1.0f);
    ASSERT_IN_RANGE(220.0f, r.x, 0.01f);
    ASSERT(r.y + r.h <= THEME_HEADER_H);
    PASS();
}

TEST notice_long_drops_below_header(void)
{
    asp_rect r = notice_pill_rect(600.0f, logo_rect(), 1.0f);
    ASSERT(r.y >= THEME_HEADER_H);
    PASS();
}

SUITE(notice_layout_suite)
{
    RUN_TEST(notice_clears_logo_and_battery);
    RUN_TEST(notice_short_stays_centred_in_header);
    RUN_TEST(notice_long_drops_below_header);
}
