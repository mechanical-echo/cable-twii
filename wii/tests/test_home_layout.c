#include "greatest/greatest.h"
#include "home_layout.h"

#define EPS 0.01f

static void centre(asp_rect r, float *x, float *y)
{
    *x = r.x + r.w / 2.0f;
    *y = r.y + r.h / 2.0f;
}

static bool apart(asp_rect a, asp_rect b)
{
    return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
}

static bool within(asp_rect inner, asp_rect outer)
{
    return inner.x >= outer.x && inner.x + inner.w <= outer.x + outer.w &&
           inner.y >= outer.y && inner.y + inner.h <= outer.y + outer.h;
}

TEST layout_bars_span_the_screen_top_and_bottom(void)
{
    home_layout l = home_layout_make();
    ASSERT_IN_RANGE(0.0f, l.top.y, EPS);
    ASSERT_IN_RANGE(640.0f, l.top.w, EPS);
    ASSERT_IN_RANGE(480.0f, l.bottom.y + l.bottom.h, EPS);
    ASSERT_IN_RANGE(640.0f, l.bottom.w, EPS);
    ASSERT(l.top.y + l.top.h <= l.wii_menu.y);
    ASSERT(l.hbc.y + l.hbc.h <= l.bottom.y);
    PASS();
}

TEST layout_pills_are_centred_and_separate(void)
{
    home_layout l = home_layout_make();
    ASSERT_IN_RANGE(320.0f, l.wii_menu.x + l.wii_menu.w / 2.0f, EPS);
    ASSERT_IN_RANGE(320.0f, l.hbc.x + l.hbc.w / 2.0f, EPS);
    ASSERT_IN_RANGE(320.0f, l.resume.x + l.resume.w / 2.0f, EPS);
    ASSERT(apart(l.wii_menu, l.hbc));
    ASSERT(apart(l.hbc, l.resume));
    ASSERT(apart(l.wii_menu, l.resume));
    PASS();
}

TEST layout_resume_and_battery_sit_in_the_bottom_bar(void)
{
    home_layout l = home_layout_make();
    ASSERT(within(l.resume, l.bottom));
    for (int i = 0; i < HOME_BATTERY_BARS; i++) {
        ASSERT(within(l.battery[i], l.bottom));
        ASSERT(apart(l.battery[i], l.resume));
    }
    PASS();
}

TEST layout_battery_bars_rise_left_to_right(void)
{
    home_layout l = home_layout_make();
    for (int i = 1; i < HOME_BATTERY_BARS; i++) {
        ASSERT(l.battery[i].x >= l.battery[i - 1].x + l.battery[i - 1].w);
        ASSERT(l.battery[i].h > l.battery[i - 1].h);
        ASSERT_IN_RANGE(l.battery[i - 1].y + l.battery[i - 1].h, l.battery[i].y + l.battery[i].h, EPS);
    }
    PASS();
}

TEST hit_finds_each_pill_at_its_centre(void)
{
    home_layout l = home_layout_make();
    float x, y;
    centre(l.wii_menu, &x, &y);
    ASSERT_EQ(HOME_BTN_WII_MENU, home_hit(&l, x, y));
    centre(l.hbc, &x, &y);
    ASSERT_EQ(HOME_BTN_HOMEBREW, home_hit(&l, x, y));
    centre(l.resume, &x, &y);
    ASSERT_EQ(HOME_BTN_RESUME, home_hit(&l, x, y));
    PASS();
}

TEST hit_misses_gaps_bars_and_corners(void)
{
    home_layout l = home_layout_make();
    float gap_y = (l.wii_menu.y + l.wii_menu.h + l.hbc.y) / 2.0f;
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 320.0f, gap_y));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 2.0f, 2.0f));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 320.0f, l.top.y + 4.0f));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, l.wii_menu.x - 4.0f, l.wii_menu.y + 4.0f));
    PASS();
}

TEST battery_maps_level_to_four_bars(void)
{
    ASSERT_EQ(0, home_battery_bars(0));
    ASSERT_EQ(1, home_battery_bars(1));
    ASSERT_EQ(1, home_battery_bars(63));
    ASSERT_EQ(2, home_battery_bars(64));
    ASSERT_EQ(2, home_battery_bars(127));
    ASSERT_EQ(3, home_battery_bars(128));
    ASSERT_EQ(3, home_battery_bars(191));
    ASSERT_EQ(4, home_battery_bars(192));
    ASSERT_EQ(4, home_battery_bars(255));
    PASS();
}

TEST dim_is_sixty_percent_black(void)
{
    ASSERT_EQ(153, HOME_DIM_ALPHA);
    PASS();
}

SUITE(home_layout_suite)
{
    RUN_TEST(layout_bars_span_the_screen_top_and_bottom);
    RUN_TEST(layout_pills_are_centred_and_separate);
    RUN_TEST(layout_resume_and_battery_sit_in_the_bottom_bar);
    RUN_TEST(layout_battery_bars_rise_left_to_right);
    RUN_TEST(hit_finds_each_pill_at_its_centre);
    RUN_TEST(hit_misses_gaps_bars_and_corners);
    RUN_TEST(battery_maps_level_to_four_bars);
    RUN_TEST(dim_is_sixty_percent_black);
}
