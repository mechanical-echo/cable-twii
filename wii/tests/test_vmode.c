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

TEST already_480_efb_with_taller_xfb_is_left_alone(void)
{
    vm_plan p = vm_plan_for(480, 576);
    ASSERT_EQ(480, p.efb_height);
    ASSERT_EQ(576, p.xfb_height);
    ASSERT_FALSE(p.rescale);
    PASS();
}

TEST pal_50_528_mode_gets_a_480_efb_with_528_copy(void)
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
    RUN_TEST(already_480_efb_with_taller_xfb_is_left_alone);
    RUN_TEST(pal_50_528_mode_gets_a_480_efb_with_528_copy);
    RUN_TEST(taller_efb_than_layout_is_rescaled);
}
