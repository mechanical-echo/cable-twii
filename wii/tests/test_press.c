#include <string.h>
#include "greatest/greatest.h"
#include "ui/press.h"

TEST press_fires_on_release_over_same_pill(void)
{
    ps_state s;
    memset(&s, 0, sizeof s);
    ASSERT_EQ(0, ps_update(&s, 2, true, true, false));
    ASSERT_EQ(0, ps_update(&s, 2, false, true, false));
    ASSERT_EQ(2, ps_update(&s, 2, false, false, true));
    ASSERT_EQ(0, ps_pressed(&s, 2));
    PASS();
}

TEST press_release_off_pill_does_not_fire(void)
{
    ps_state s;
    memset(&s, 0, sizeof s);
    ps_update(&s, 2, true, true, false);
    ASSERT_EQ(0, ps_update(&s, 0, false, false, true));
    ASSERT_EQ(0, s.armed);
    ASSERT_EQ(0, ps_update(&s, 2, false, false, true));
    PASS();
}

TEST press_started_elsewhere_does_not_fire(void)
{
    ps_state s;
    memset(&s, 0, sizeof s);
    ps_update(&s, 0, true, true, false);
    ASSERT_EQ(0, ps_update(&s, 3, false, true, false));
    ASSERT_EQ(0, ps_update(&s, 3, false, false, true));
    PASS();
}

TEST pressed_only_while_held_on_armed_pill(void)
{
    ps_state s;
    memset(&s, 0, sizeof s);
    ASSERT_EQ(0, ps_pressed(&s, 2));
    ps_update(&s, 2, true, true, false);
    ASSERT_EQ(2, ps_pressed(&s, 2));
    ASSERT_EQ(0, ps_pressed(&s, 1));
    ps_update(&s, 1, false, true, false);
    ASSERT_EQ(0, ps_pressed(&s, 1));
    ASSERT_EQ(2, ps_pressed(&s, 2));
    ps_update(&s, 0, false, false, false);
    ASSERT_EQ(0, ps_pressed(&s, 2));
    PASS();
}

SUITE(press_suite)
{
    RUN_TEST(press_fires_on_release_over_same_pill);
    RUN_TEST(press_release_off_pill_does_not_fire);
    RUN_TEST(press_started_elsewhere_does_not_fire);
    RUN_TEST(pressed_only_while_held_on_armed_pill);
}
