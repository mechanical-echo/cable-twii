#include "greatest/greatest.h"
#include "ui/tween.h"

TEST ease_out_endpoints(void)
{
    ASSERT_IN_RANGE(0.0f, tw_ease_out(0.0f), 0.0001f);
    ASSERT_IN_RANGE(1.0f, tw_ease_out(1.0f), 0.0001f);
    ASSERT_IN_RANGE(0.875f, tw_ease_out(0.5f), 0.0001f);
    PASS();
}

TEST ease_out_clamps(void)
{
    ASSERT_IN_RANGE(1.0f, tw_ease_out(2.0f), 0.0001f);
    ASSERT_IN_RANGE(0.0f, tw_ease_out(-1.0f), 0.0001f);
    PASS();
}

TEST tween_progress(void)
{
    tween t = tw_make(10, 20, 1000, 100);
    ASSERT_IN_RANGE(10.0f, tw_value(&t, 1000), 0.001f);
    ASSERT_IN_RANGE(20.0f, tw_value(&t, 1100), 0.001f);
    ASSERT(tw_done(&t, 1100));
    ASSERT(!tw_done(&t, 1050));
    PASS();
}

SUITE(tween_suite)
{
    RUN_TEST(ease_out_endpoints);
    RUN_TEST(ease_out_clamps);
    RUN_TEST(tween_progress);
}
