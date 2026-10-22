#include "greatest/greatest.h"
#include "vmode.h"

TEST ntsc_and_eurgb60_480_are_left_alone(void)
{
    vm_plan p = vm_plan_for(480, 480);
    ASSERT_EQ(480, p.efb_height);
    ASSERT_EQ(480, p.xfb_height);
    ASSERT_FALSE(p.rescale);
    PASS();
}

TEST pal_576_scale_mode_is_already_480_efb(void)
{
    vm_plan p = vm_plan_for(480, 576);
    ASSERT_EQ(480, p.efb_height);
    ASSERT_EQ(576, p.xfb_height);
    ASSERT_FALSE(p.rescale);
    PASS();
}

TEST pal_528_gets_a_480_efb_scaled_to_528(void)
{
    vm_plan p = vm_plan_for(528, 528);
    ASSERT_EQ(VM_LOGICAL_HEIGHT, p.efb_height);
    ASSERT_EQ(528, p.xfb_height);
    ASSERT(p.rescale);
    PASS();
}

TEST taller_efb_than_layout_is_rescaled(void)
{
    vm_plan p = vm_plan_for(574, 574);
    ASSERT_EQ(480, p.efb_height);
    ASSERT_EQ(574, p.xfb_height);
    ASSERT(p.rescale);
    PASS();
}

SUITE(vmode_suite)
{
    RUN_TEST(ntsc_and_eurgb60_480_are_left_alone);
    RUN_TEST(pal_576_scale_mode_is_already_480_efb);
    RUN_TEST(pal_528_gets_a_480_efb_scaled_to_528);
    RUN_TEST(taller_efb_than_layout_is_rescaled);
}
