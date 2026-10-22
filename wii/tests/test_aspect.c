#include "greatest/greatest.h"
#include "aspect.h"

#define EPS 0.01f

TEST pixel_square_on_4_3(void)
{
    ASSERT_IN_RANGE(1.0f, asp_pixel_x(false), EPS);
    PASS();
}

TEST pixel_narrowed_on_16_9(void)
{
    ASSERT_IN_RANGE(0.75f, asp_pixel_x(true), EPS);
    PASS();
}

TEST src_4_3_fills_4_3_tv(void)
{
    asp_rect r = asp_fit(4.0f / 3.0f, false, 0, 0, 640, 480);
    ASSERT_IN_RANGE(0.0f, r.x, EPS);
    ASSERT_IN_RANGE(0.0f, r.y, EPS);
    ASSERT_IN_RANGE(640.0f, r.w, EPS);
    ASSERT_IN_RANGE(480.0f, r.h, EPS);
    PASS();
}

TEST src_4_3_pillarboxed_on_16_9_tv(void)
{
    asp_rect r = asp_fit(4.0f / 3.0f, true, 0, 0, 640, 480);
    ASSERT_IN_RANGE(80.0f, r.x, EPS);
    ASSERT_IN_RANGE(0.0f, r.y, EPS);
    ASSERT_IN_RANGE(480.0f, r.w, EPS);
    ASSERT_IN_RANGE(480.0f, r.h, EPS);
    PASS();
}

TEST src_16_9_fills_16_9_tv(void)
{
    asp_rect r = asp_fit(16.0f / 9.0f, true, 0, 0, 640, 480);
    ASSERT_IN_RANGE(0.0f, r.x, EPS);
    ASSERT_IN_RANGE(0.0f, r.y, EPS);
    ASSERT_IN_RANGE(640.0f, r.w, EPS);
    ASSERT_IN_RANGE(480.0f, r.h, EPS);
    PASS();
}

TEST src_16_9_letterboxed_on_4_3_tv(void)
{
    asp_rect r = asp_fit(16.0f / 9.0f, false, 0, 0, 640, 480);
    ASSERT_IN_RANGE(0.0f, r.x, EPS);
    ASSERT_IN_RANGE(60.0f, r.y, EPS);
    ASSERT_IN_RANGE(640.0f, r.w, EPS);
    ASSERT_IN_RANGE(360.0f, r.h, EPS);
    PASS();
}

TEST fits_inside_offset_rect(void)
{
    asp_rect r = asp_fit(4.0f / 3.0f, true, 100, 50, 320, 180);
    ASSERT_IN_RANGE(100.0f + (320.0f - 180.0f) / 2.0f, r.x, EPS);
    ASSERT_IN_RANGE(50.0f, r.y, EPS);
    ASSERT_IN_RANGE(180.0f, r.w, EPS);
    ASSERT_IN_RANGE(180.0f, r.h, EPS);
    PASS();
}

TEST bad_aspect_fills_rect(void)
{
    asp_rect r = asp_fit(0.0f, true, 10, 20, 300, 200);
    ASSERT_IN_RANGE(10.0f, r.x, EPS);
    ASSERT_IN_RANGE(20.0f, r.y, EPS);
    ASSERT_IN_RANGE(300.0f, r.w, EPS);
    ASSERT_IN_RANGE(200.0f, r.h, EPS);
    PASS();
}

SUITE(aspect_suite)
{
    RUN_TEST(pixel_square_on_4_3);
    RUN_TEST(pixel_narrowed_on_16_9);
    RUN_TEST(src_4_3_fills_4_3_tv);
    RUN_TEST(src_4_3_pillarboxed_on_16_9_tv);
    RUN_TEST(src_16_9_fills_16_9_tv);
    RUN_TEST(src_16_9_letterboxed_on_4_3_tv);
    RUN_TEST(fits_inside_offset_rect);
    RUN_TEST(bad_aspect_fills_rect);
}
