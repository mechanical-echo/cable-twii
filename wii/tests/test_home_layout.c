#include "greatest/greatest.h"
#include "home_layout.h"

#define EPS 0.2f
#define WIDE 0.75f

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
    return inner.x >= outer.x - EPS && inner.x + inner.w <= outer.x + outer.w + EPS &&
           inner.y >= outer.y - EPS && inner.y + inner.h <= outer.y + outer.h + EPS;
}

TEST layout_bars_span_the_screen_top_and_bottom(void)
{
    home_layout l = home_layout_make(WIDE);
    ASSERT_IN_RANGE(0.0f, l.top.y, EPS);
    ASSERT_IN_RANGE(640.0f, l.top.w, EPS);
    ASSERT_IN_RANGE(72.0f, l.top.h, EPS);
    ASSERT_IN_RANGE(408.0f, l.bottom.y, EPS);
    ASSERT_IN_RANGE(480.0f, l.bottom.y + l.bottom.h, EPS);
    ASSERT_IN_RANGE(640.0f, l.bottom.w, EPS);
    ASSERT(l.top.y + l.top.h <= l.wii_menu.y);
    PASS();
}

TEST layout_pills_match_the_deck(void)
{
    home_layout l = home_layout_make(WIDE);
    ASSERT_IN_RANGE(150.0f, l.wii_menu.x, EPS);
    ASSERT_IN_RANGE(133.3f, l.wii_menu.y, EPS);
    ASSERT_IN_RANGE(340.0f, l.wii_menu.w, EPS);
    ASSERT_IN_RANGE(60.0f, l.wii_menu.h, EPS);
    ASSERT_IN_RANGE(221.3f, l.hbc.y, EPS);
    ASSERT_IN_RANGE(225.0f, l.resume.x, EPS);
    ASSERT_IN_RANGE(311.1f, l.resume.y, EPS);
    ASSERT_IN_RANGE(190.0f, l.resume.w, EPS);
    ASSERT_IN_RANGE(40.0f, l.resume.h, EPS);
    ASSERT(apart(l.wii_menu, l.hbc));
    ASSERT(apart(l.hbc, l.resume));
    PASS();
}

TEST layout_resume_sits_above_the_bottom_bar(void)
{
    home_layout l = home_layout_make(WIDE);
    ASSERT(l.resume.y + l.resume.h <= l.bottom.y);
    ASSERT(apart(l.resume, l.bottom));
    PASS();
}

TEST layout_badge_title_and_playing_sit_in_the_top_bar(void)
{
    home_layout l = home_layout_make(WIDE);
    ASSERT_IN_RANGE(26.0f, l.badge.x, EPS);
    ASSERT_IN_RANGE(16.9f, l.badge.y, EPS);
    ASSERT_IN_RANGE(37.0f, l.badge.h, EPS);
    ASSERT_IN_RANGE(27.75f, l.badge.w, EPS);
    ASSERT_IN_RANGE(62.0f, l.title.x, 1.0f);
    ASSERT_IN_RANGE(22.4f, l.title.y, EPS);
    ASSERT_IN_RANGE(614.0f, l.playing.x + l.playing.w, EPS);
    ASSERT_IN_RANGE(27.6f, l.playing.y, EPS);
    ASSERT(within(l.badge, l.top) && within(l.title, l.top) && within(l.playing, l.top));
    PASS();
}

TEST home_4_3_title_clears_badge(void)
{
    home_layout l = home_layout_make(1.0f);
    ASSERT_IN_RANGE(37.0f, l.badge.w, EPS);
    ASSERT(l.title.x >= l.badge.x + l.badge.w);
    PASS();
}

TEST layout_battery_and_volume_sit_in_the_bottom_bar(void)
{
    home_layout l = home_layout_make(WIDE);
    ASSERT_IN_RANGE(26.0f, l.vol_label.x, EPS);
    ASSERT_IN_RANGE(436.4f, l.vol_label.y, EPS);
    ASSERT_IN_RANGE(68.2f, l.vol_bar.x, EPS);
    ASSERT_IN_RANGE(436.4f, l.vol_bar.y, EPS);
    ASSERT_IN_RANGE(121.0f, l.vol_bar.w, EPS);
    ASSERT_IN_RANGE(16.0f, l.vol_bar.h, EPS);
    ASSERT_IN_RANGE(554.1f, l.battery_label.x, EPS);
    ASSERT_IN_RANGE(572.3f, l.battery.x, EPS);
    ASSERT_IN_RANGE(431.1f, l.battery.y, EPS);
    ASSERT_IN_RANGE(26.7f, l.battery.h, EPS);
    ASSERT(within(l.vol_label, l.bottom) && within(l.vol_bar, l.bottom) && within(l.battery, l.bottom));
    ASSERT(apart(l.vol_bar, l.battery));
    PASS();
}

TEST layout_segments_fit_the_volume_bar(void)
{
    home_layout l = home_layout_make(WIDE);
    ASSERT_IN_RANGE(10.0f, HOME_SEGMENT_W, EPS);
    ASSERT_IN_RANGE(2.35f, HOME_SEGMENT_GAP, EPS);
    ASSERT_IN_RANGE(10.0f * 10.0f + 2.35f * 9.0f, l.vol_bar.w, 0.5f);
    PASS();
}

TEST hit_finds_each_pill_at_its_centre(void)
{
    home_layout l = home_layout_make(WIDE);
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
    home_layout l = home_layout_make(WIDE);
    float gap_y = (l.wii_menu.y + l.wii_menu.h + l.hbc.y) / 2.0f;
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 320.0f, gap_y));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 2.0f, 2.0f));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 320.0f, l.top.y + 4.0f));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, l.wii_menu.x - 4.0f, l.wii_menu.y + 4.0f));
    ASSERT_EQ(HOME_BTN_NONE, home_hit(&l, 320.0f, l.bottom.y + 10.0f));
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
    RUN_TEST(layout_pills_match_the_deck);
    RUN_TEST(layout_resume_sits_above_the_bottom_bar);
    RUN_TEST(layout_badge_title_and_playing_sit_in_the_top_bar);
    RUN_TEST(home_4_3_title_clears_badge);
    RUN_TEST(layout_battery_and_volume_sit_in_the_bottom_bar);
    RUN_TEST(layout_segments_fit_the_volume_bar);
    RUN_TEST(hit_finds_each_pill_at_its_centre);
    RUN_TEST(hit_misses_gaps_bars_and_corners);
    RUN_TEST(battery_maps_level_to_four_bars);
    RUN_TEST(dim_is_sixty_percent_black);
}
