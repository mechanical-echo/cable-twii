#include "greatest/greatest.h"
#include "sysbar_layout.h"

#define EPS 0.2f

static void centre(asp_rect r, float *x, float *y)
{
    *x = r.x + r.w / 2.0f;
    *y = r.y + r.h / 2.0f;
}

TEST sysbar_body_and_hump(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    ASSERT_IN_RANGE(0.0f, l.body.x, EPS);
    ASSERT_IN_RANGE(433.0f, l.body.y, EPS);
    ASSERT_IN_RANGE(640.0f, l.body.w, EPS);
    ASSERT_IN_RANGE(47.0f, l.body.h, EPS);
    ASSERT_IN_RANGE(415.0f, l.hump.y, EPS);
    ASSERT_IN_RANGE(79.0f, l.hump.w, EPS);
    ASSERT_IN_RANGE(18.0f, l.hump.h, EPS);
    ASSERT_IN_RANGE(320.0f, l.hump.x + l.hump.w / 2.0f, EPS);
    ASSERT_IN_RANGE(47.0f, l.slope_w, EPS);
    PASS();
}

TEST sysbar_top_follows_the_curve(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    ASSERT_IN_RANGE(433.0f, sysbar_top_at(&l, 100.0f), EPS);
    ASSERT_IN_RANGE(433.0f, sysbar_top_at(&l, 600.0f), EPS);
    ASSERT_IN_RANGE(415.0f, sysbar_top_at(&l, 320.0f), EPS);
    float left_mid = sysbar_top_at(&l, 257.0f);
    ASSERT(left_mid > 415.5f && left_mid < 432.5f);
    float right_mid = sysbar_top_at(&l, 383.0f);
    ASSERT(right_mid > 415.5f && right_mid < 432.5f);
    for (float x = 233.0f; x < 281.0f; x += 2.0f) ASSERT(sysbar_top_at(&l, x + 2.0f) <= sysbar_top_at(&l, x) + 0.001f);
    for (float x = 359.0f; x < 407.0f; x += 2.0f) ASSERT(sysbar_top_at(&l, x + 2.0f) >= sysbar_top_at(&l, x) - 0.001f);
    PASS();
}

TEST sysbar_clock_and_date_are_centred(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    ASSERT_IN_RANGE(320.0f, l.clock.x + l.clock.w / 2.0f, EPS);
    ASSERT_IN_RANGE(420.0f, l.clock.y, EPS);
    ASSERT_IN_RANGE(28.0f, l.clock.h, EPS);
    ASSERT_IN_RANGE(320.0f, l.date.x + l.date.w / 2.0f, EPS);
    ASSERT_IN_RANGE(453.0f, l.date.y, EPS);
    PASS();
}

TEST sysbar_hit_finds_each_button(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    float x, y;
    centre(l.home_disc, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_HOME, sysbar_hit(&l, x, y, true));
    centre(l.home_label, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_HOME, sysbar_hit(&l, x, y, true));
    centre(l.prev, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_PREV, sysbar_hit(&l, x, y, true));
    centre(l.next, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_NEXT, sysbar_hit(&l, x, y, true));
    PASS();
}

TEST sysbar_hides_channel_pills(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    float x, y;
    centre(l.prev, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_NONE, sysbar_hit(&l, x, y, false));
    centre(l.next, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_NONE, sysbar_hit(&l, x, y, false));
    centre(l.home_disc, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_HOME, sysbar_hit(&l, x, y, false));
    PASS();
}

TEST sysbar_4_3_label_clears_disc(void)
{
    sysbar_layout wide = sysbar_layout_for(0.75f);
    ASSERT_IN_RANGE(21.3f, wide.home_disc.x, EPS);
    ASSERT_IN_RANGE(437.3f, wide.home_disc.y, EPS);
    ASSERT_IN_RANGE(37.0f, wide.home_disc.h, EPS);
    ASSERT_IN_RANGE(27.75f, wide.home_disc.w, EPS);
    ASSERT_IN_RANGE(55.3f, wide.home_label.x, 1.0f);
    sysbar_layout narrow = sysbar_layout_for(1.0f);
    ASSERT_IN_RANGE(37.0f, narrow.home_disc.w, EPS);
    ASSERT(narrow.home_label.x >= narrow.home_disc.x + narrow.home_disc.w);
    PASS();
}

TEST sysbar_pills_sit_at_the_right(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    ASSERT_IN_RANGE(496.7f, l.prev.x, EPS);
    ASSERT_IN_RANGE(560.7f, l.next.x, EPS);
    ASSERT_IN_RANGE(440.9f, l.prev.y, EPS);
    ASSERT_IN_RANGE(58.0f, l.prev.w, EPS);
    ASSERT_IN_RANGE(32.0f, l.prev.h, EPS);
    ASSERT(l.prev.x + l.prev.w <= l.next.x);
    PASS();
}

TEST sysbar_hit_misses_the_clock(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    float x, y;
    centre(l.clock, &x, &y);
    ASSERT_EQ(SYSBAR_BTN_NONE, sysbar_hit(&l, x, y, true));
    ASSERT_EQ(SYSBAR_BTN_NONE, sysbar_hit(&l, 5.0f, 5.0f, true));
    ASSERT_EQ(SYSBAR_BTN_NONE, sysbar_hit(&l, l.home_label.x + l.home_label.w + 20.0f, l.home_label.y + 2.0f, true));
    PASS();
}

TEST sysbar_shift_moves_everything_down(void)
{
    sysbar_layout l = sysbar_layout_for(0.75f);
    sysbar_layout s = sysbar_layout_shift(&l, 40.0f);
    ASSERT_IN_RANGE(l.body.y + 40.0f, s.body.y, EPS);
    ASSERT_IN_RANGE(l.hump.y + 40.0f, s.hump.y, EPS);
    ASSERT_IN_RANGE(l.clock.y + 40.0f, s.clock.y, EPS);
    ASSERT_IN_RANGE(l.date.y + 40.0f, s.date.y, EPS);
    ASSERT_IN_RANGE(l.home_disc.y + 40.0f, s.home_disc.y, EPS);
    ASSERT_IN_RANGE(l.home_label.y + 40.0f, s.home_label.y, EPS);
    ASSERT_IN_RANGE(l.home_hit.y + 40.0f, s.home_hit.y, EPS);
    ASSERT_IN_RANGE(l.prev.y + 40.0f, s.prev.y, EPS);
    ASSERT_IN_RANGE(l.next.y + 40.0f, s.next.y, EPS);
    ASSERT_IN_RANGE(l.prev.x, s.prev.x, EPS);
    ASSERT_IN_RANGE(sysbar_top_at(&l, 320.0f) + 40.0f, sysbar_top_at(&s, 320.0f), EPS);
    PASS();
}

SUITE(sysbar_layout_suite)
{
    RUN_TEST(sysbar_shift_moves_everything_down);
    RUN_TEST(sysbar_body_and_hump);
    RUN_TEST(sysbar_top_follows_the_curve);
    RUN_TEST(sysbar_clock_and_date_are_centred);
    RUN_TEST(sysbar_hit_finds_each_button);
    RUN_TEST(sysbar_hides_channel_pills);
    RUN_TEST(sysbar_4_3_label_clears_disc);
    RUN_TEST(sysbar_pills_sit_at_the_right);
    RUN_TEST(sysbar_hit_misses_the_clock);
}
