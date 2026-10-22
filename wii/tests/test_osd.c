#include <string.h>
#include "greatest/greatest.h"
#include "osd.h"

TEST chip_shows_two_seconds(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_channel_changed(&s, 1000);
    ASSERT(osd_chip_visible(&s, 2999));
    ASSERT(!osd_chip_visible(&s, 3000));
    PASS();
}

TEST osd_shows_on_motion(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    ASSERT(osd_visible(&s, 3999));
    ASSERT(!osd_visible(&s, 4000));
    PASS();
}

TEST osd_hidden_without_ir(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    osd_pointer(&s, false, false, 1100);
    ASSERT(!osd_visible(&s, 1100));
    PASS();
}

TEST osd_still_pointer_times_out(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    osd_pointer(&s, true, false, 2000);
    ASSERT(!osd_visible(&s, 4000));
    PASS();
}

TEST volume_step_moves_by_ten(void)
{
    ASSERT_EQ(90, osd_volume_step(100, -1));
    ASSERT_EQ(50, osd_volume_step(40, 1));
    ASSERT_EQ(0, osd_volume_step(10, -1));
    PASS();
}

TEST volume_step_clamps(void)
{
    ASSERT_EQ(100, osd_volume_step(100, 1));
    ASSERT_EQ(0, osd_volume_step(0, -1));
    ASSERT_EQ(100, osd_volume_step(95, 1));
    ASSERT_EQ(0, osd_volume_step(5, -1));
    PASS();
}

TEST volume_step_snaps_to_tens(void)
{
    ASSERT_EQ(80, osd_volume_step(75, 1));
    ASSERT_EQ(70, osd_volume_step(75, -1));
    PASS();
}

TEST volume_bar_shows_alone_for_two_seconds(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    ASSERT(!osd_volume_alone(&s, 0));
    osd_volume_changed(&s, 1000);
    ASSERT(osd_volume_alone(&s, 2999));
    ASSERT(!osd_volume_alone(&s, 3000));
    PASS();
}

TEST volume_bar_yields_to_panel(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_volume_changed(&s, 1000);
    osd_pointer(&s, true, true, 1100);
    ASSERT(!osd_volume_alone(&s, 1200));
    osd_pointer(&s, false, false, 1300);
    ASSERT(osd_volume_alone(&s, 1300));
    PASS();
}

TEST activity_extends_visible_panel(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    osd_activity(&s, 3500);
    ASSERT(osd_visible(&s, 6499));
    ASSERT(!osd_visible(&s, 6500));
    PASS();
}

TEST activity_does_not_show_hidden_panel(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_activity(&s, 1000);
    ASSERT(!osd_visible(&s, 1000));
    osd_pointer(&s, true, true, 1000);
    osd_pointer(&s, false, false, 1100);
    osd_activity(&s, 1200);
    ASSERT(!osd_visible(&s, 1200));
    PASS();
}

TEST slide_eases_out_over_200ms(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    ASSERT_IN_RANGE(0.0f, osd_slide(&s, 1000), 0.001f);
    ASSERT_IN_RANGE(0.875f, osd_slide(&s, 1100), 0.001f);
    ASSERT_IN_RANGE(1.0f, osd_slide(&s, 1200), 0.001f);
    ASSERT_IN_RANGE(1.0f, osd_slide(&s, 2500), 0.001f);
    PASS();
}

TEST slide_restarts_after_hiding(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    osd_pointer(&s, true, true, 1500);
    ASSERT_IN_RANGE(1.0f, osd_slide(&s, 1500), 0.001f);
    osd_pointer(&s, false, false, 1600);
    osd_pointer(&s, true, true, 2000);
    ASSERT_IN_RANGE(0.0f, osd_slide(&s, 2000), 0.001f);
    osd_pointer(&s, true, true, 6000);
    ASSERT_IN_RANGE(0.0f, osd_slide(&s, 6000), 0.001f);
    PASS();
}

SUITE(osd_suite)
{
    RUN_TEST(chip_shows_two_seconds);
    RUN_TEST(osd_shows_on_motion);
    RUN_TEST(osd_hidden_without_ir);
    RUN_TEST(osd_still_pointer_times_out);
    RUN_TEST(volume_step_moves_by_ten);
    RUN_TEST(volume_step_clamps);
    RUN_TEST(volume_step_snaps_to_tens);
    RUN_TEST(volume_bar_shows_alone_for_two_seconds);
    RUN_TEST(volume_bar_yields_to_panel);
    RUN_TEST(activity_extends_visible_panel);
    RUN_TEST(activity_does_not_show_hidden_panel);
    RUN_TEST(slide_eases_out_over_200ms);
    RUN_TEST(slide_restarts_after_hiding);
}
