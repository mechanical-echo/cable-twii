#include <math.h>
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

#define WIDE_SRC (16.0f / 9.0f)
#define STD_SRC (4.0f / 3.0f)

static bool rect_is(asp_rect r, float x, float y, float w, float h)
{
    return fabsf(r.x - x) < EPS && fabsf(r.y - y) < EPS && fabsf(r.w - w) < EPS && fabsf(r.h - h) < EPS;
}

TEST shown_on_4_3_tv_for_16_9_show(void)
{
    ASSERT_IN_RANGE(1.778f, asp_shown(WIDE_SRC, false, ASP_FIT, 1.0f), EPS);
    ASSERT_IN_RANGE(1.556f, asp_shown(WIDE_SRC, false, ASP_14_9, 1.0f), EPS);
    ASSERT_IN_RANGE(1.333f, asp_shown(WIDE_SRC, false, ASP_FILL, 1.0f), EPS);
    PASS();
}

TEST shown_leaves_matching_shows(void)
{
    for (int m = 0; m < ASP_MODES; m++) {
        ASSERT_IN_RANGE(1.333f, asp_shown(STD_SRC, false, (asp_mode)m, 1.0f), EPS);
        ASSERT_IN_RANGE(1.778f, asp_shown(WIDE_SRC, true, (asp_mode)m, 1.0f), EPS);
    }
    PASS();
}

TEST shown_on_16_9_tv_for_4_3_show(void)
{
    ASSERT_IN_RANGE(1.333f, asp_shown(STD_SRC, true, ASP_FIT, 1.0f), EPS);
    ASSERT_IN_RANGE(1.556f, asp_shown(STD_SRC, true, ASP_14_9, 1.0f), EPS);
    ASSERT_IN_RANGE(1.778f, asp_shown(STD_SRC, true, ASP_FILL, 1.0f), EPS);
    PASS();
}

TEST shown_14_9_stays_between_show_and_tv(void)
{
    ASSERT_IN_RANGE(1.556f, asp_shown(2.35f, false, ASP_14_9, 1.0f), EPS);
    ASSERT_IN_RANGE(1.333f, asp_shown(1.2f, false, ASP_14_9, 1.0f), EPS);
    PASS();
}

TEST shown_eases_with_amount(void)
{
    ASSERT_IN_RANGE(1.778f, asp_shown(WIDE_SRC, false, ASP_FILL, 0.0f), EPS);
    ASSERT_IN_RANGE(1.667f, asp_shown(WIDE_SRC, false, ASP_FILL, 0.25f), EPS);
    ASSERT_IN_RANGE(1.333f, asp_shown(WIDE_SRC, false, ASP_FILL, 1.0f), EPS);
    ASSERT_IN_RANGE(1.333f, asp_shown(WIDE_SRC, false, ASP_FILL, 2.0f), EPS);
    ASSERT_IN_RANGE(1.778f, asp_shown(WIDE_SRC, false, ASP_FILL, -1.0f), EPS);
    PASS();
}

TEST shown_keeps_bad_aspect(void)
{
    ASSERT_IN_RANGE(0.0f, asp_shown(0.0f, false, ASP_FILL, 1.0f), EPS);
    PASS();
}

TEST crop_cuts_sides_of_16_9_on_4_3(void)
{
    ASSERT(rect_is(asp_crop(WIDE_SRC, STD_SRC), 0.125f, 0.0f, 0.75f, 1.0f));
    ASSERT(rect_is(asp_crop(WIDE_SRC, 14.0f / 9.0f), 0.0625f, 0.0f, 0.875f, 1.0f));
    PASS();
}

TEST crop_cuts_top_and_bottom_of_4_3_on_16_9(void)
{
    ASSERT(rect_is(asp_crop(STD_SRC, WIDE_SRC), 0.0f, 0.125f, 1.0f, 0.75f));
    ASSERT(rect_is(asp_crop(STD_SRC, 14.0f / 9.0f), 0.0f, 0.0714f, 1.0f, 0.857f));
    PASS();
}

TEST crop_bad_aspect_is_whole_frame(void)
{
    ASSERT(rect_is(asp_crop(WIDE_SRC, WIDE_SRC), 0.0f, 0.0f, 1.0f, 1.0f));
    ASSERT(rect_is(asp_crop(0.0f, STD_SRC), 0.0f, 0.0f, 1.0f, 1.0f));
    ASSERT(rect_is(asp_crop(WIDE_SRC, 0.0f), 0.0f, 0.0f, 1.0f, 1.0f));
    PASS();
}

TEST fourteen_nine_fits_4_3_tv(void)
{
    asp_rect r = asp_fit(14.0f / 9.0f, false, 0, 0, 640, 480);
    ASSERT(rect_is(r, 0.0f, 34.29f, 640.0f, 411.43f));
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
    RUN_TEST(shown_on_4_3_tv_for_16_9_show);
    RUN_TEST(shown_leaves_matching_shows);
    RUN_TEST(shown_on_16_9_tv_for_4_3_show);
    RUN_TEST(shown_14_9_stays_between_show_and_tv);
    RUN_TEST(shown_eases_with_amount);
    RUN_TEST(shown_keeps_bad_aspect);
    RUN_TEST(crop_cuts_sides_of_16_9_on_4_3);
    RUN_TEST(crop_cuts_top_and_bottom_of_4_3_on_16_9);
    RUN_TEST(crop_bad_aspect_is_whole_frame);
    RUN_TEST(fourteen_nine_fits_4_3_tv);
}
