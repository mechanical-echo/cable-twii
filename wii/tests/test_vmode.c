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

TEST copy_lines_match_libogc(void)
{
    ASSERT_EQ(480, vm_copy_lines(480, 256));
    ASSERT_EQ(529, vm_copy_lines(480, 232));
    ASSERT_EQ(527, vm_copy_lines(480, 233));
    PASS();
}

TEST pal_50_copy_fits_the_528_line_buffer(void)
{
    vm_plan p = vm_plan_for(528, 528);
    ASSERT(p.copy_lines <= 528);
    ASSERT_EQ(p.copy_lines, vm_copy_lines(VM_LOGICAL_HEIGHT, p.scale_reg));
    ASSERT(vm_copy_lines(VM_LOGICAL_HEIGHT, p.scale_reg - 1) > 528);
    PASS();
}

TEST taller_copy_fits_its_buffer(void)
{
    vm_plan p = vm_plan_for(574, 574);
    ASSERT(p.copy_lines <= 574);
    ASSERT(vm_copy_lines(VM_LOGICAL_HEIGHT, p.scale_reg - 1) > 574);
    PASS();
}

TEST unscaled_copy_is_one_to_one(void)
{
    vm_plan p = vm_plan_for(480, 480);
    ASSERT_EQ(256u, p.scale_reg);
    ASSERT_EQ(480, p.copy_lines);
    PASS();
}

SUITE(vmode_suite)
{
    RUN_TEST(ntsc_and_eurgb60_480_are_left_alone);
    RUN_TEST(already_480_efb_with_taller_xfb_is_left_alone);
    RUN_TEST(pal_50_528_mode_gets_a_480_efb_with_528_copy);
    RUN_TEST(taller_efb_than_layout_is_rescaled);
    RUN_TEST(copy_lines_match_libogc);
    RUN_TEST(pal_50_copy_fits_the_528_line_buffer);
    RUN_TEST(taller_copy_fits_its_buffer);
    RUN_TEST(unscaled_copy_is_one_to_one);
}
