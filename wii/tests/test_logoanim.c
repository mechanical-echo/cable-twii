#include "greatest/greatest.h"
#include "logoanim.h"

#define EPS 0.01f
#define IMG_W 256.0f
#define IMG_H 64.0f

TEST logo_header_rect_fits_header(void)
{
    float x, y, w, h;
    logo_header_rect_for(IMG_W, IMG_H, &x, &y, &w, &h);
    ASSERT_IN_RANGE(19.3f, x, EPS);
    ASSERT_IN_RANGE(11.5f, y, EPS);
    ASSERT_IN_RANGE(100.0f, w, EPS);
    ASSERT_IN_RANGE(25.0f, h, EPS);
    PASS();
}

TEST logo_starts_small_and_clear(void)
{
    logo_frame f = logo_frame_at(0, false, IMG_W, IMG_H);
    ASSERT_IN_RANGE(171.18f, f.w, EPS);
    ASSERT_IN_RANGE(42.795f, f.h, EPS);
    ASSERT_IN_RANGE(320.0f - 85.59f, f.x, EPS);
    ASSERT_IN_RANGE(231.1f - 21.3975f, f.y, EPS);
    ASSERT_IN_RANGE(f.w / IMG_W, f.scale, EPS);
    ASSERT_IN_RANGE(0.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.panel_alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.shine, EPS);
    ASSERT_IN_RANGE(199.98f, f.pw, EPS);
    ASSERT_IN_RANGE(74.64f, f.ph, EPS);
    ASSERT_IN_RANGE(320.0f - 99.99f, f.px, EPS);
    ASSERT_IN_RANGE(231.1f - 37.32f, f.py, EPS);
    ASSERT_FALSE(f.done);
    PASS();
}

TEST logo_scales_in_with_ease_out(void)
{
    logo_frame f = logo_frame_at(300, false, IMG_W, IMG_H);
    ASSERT_IN_RANGE(0.875f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.875f, f.panel_alpha, EPS);
    ASSERT_IN_RANGE(285.3f * 0.95f, f.w, EPS);
    ASSERT_IN_RANGE(333.3f * 0.95f, f.pw, EPS);
    ASSERT_IN_RANGE(0.0f, f.shine, EPS);
    PASS();
}

TEST logo_is_full_size_at_600(void)
{
    logo_frame f = logo_frame_at(600, false, IMG_W, IMG_H);
    ASSERT_IN_RANGE(285.3f, f.w, EPS);
    ASSERT_IN_RANGE(71.325f, f.h, EPS);
    ASSERT_IN_RANGE(177.35f, f.x, EPS);
    ASSERT_IN_RANGE(231.1f - 35.6625f, f.y, EPS);
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(1.0f, f.panel_alpha, EPS);
    ASSERT_IN_RANGE(153.35f, f.px, EPS);
    ASSERT_IN_RANGE(168.9f, f.py, EPS);
    ASSERT_IN_RANGE(333.3f, f.pw, EPS);
    ASSERT_IN_RANGE(124.4f, f.ph, EPS);
    ASSERT_IN_RANGE(0.0f, f.shine, EPS);
    PASS();
}

TEST logo_shine_sweeps_600_to_1400(void)
{
    logo_frame mid = logo_frame_at(1000, false, IMG_W, IMG_H);
    ASSERT_IN_RANGE(0.875f, mid.shine, EPS);
    ASSERT_IN_RANGE(177.35f, mid.x, EPS);
    ASSERT_IN_RANGE(1.0f, mid.panel_alpha, EPS);
    logo_frame end = logo_frame_at(1400, false, IMG_W, IMG_H);
    ASSERT_IN_RANGE(1.0f, end.shine, EPS);
    ASSERT_IN_RANGE(177.35f, end.x, EPS);
    ASSERT_IN_RANGE(285.3f, end.w, EPS);
    ASSERT_FALSE(end.done);
    PASS();
}

TEST logo_moves_to_header_1400_to_2000(void)
{
    logo_frame f = logo_frame_at(1700, false, IMG_W, IMG_H);
    ASSERT_IN_RANGE(177.35f + (19.3f - 177.35f) * 0.875f, f.x, EPS);
    ASSERT_IN_RANGE(195.4375f + (11.5f - 195.4375f) * 0.875f, f.y, EPS);
    ASSERT_IN_RANGE(285.3f + (100.0f - 285.3f) * 0.875f, f.w, EPS);
    ASSERT_IN_RANGE(71.325f + (25.0f - 71.325f) * 0.875f, f.h, EPS);
    ASSERT_IN_RANGE(f.w / IMG_W, f.scale, EPS);
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.125f, f.panel_alpha, EPS);
    float k = f.w / 285.3f;
    ASSERT_IN_RANGE(333.3f * k, f.pw, EPS);
    ASSERT_IN_RANGE(124.4f * k, f.ph, EPS);
    ASSERT_IN_RANGE(f.x + f.w / 2.0f, f.px + f.pw / 2.0f, EPS);
    ASSERT_IN_RANGE(f.y + f.h / 2.0f, f.py + f.ph / 2.0f, EPS);
    ASSERT_IN_RANGE(1.0f, f.shine, EPS);
    ASSERT_FALSE(f.done);
    ASSERT_FALSE(logo_frame_at(1999, false, IMG_W, IMG_H).done);
    PASS();
}

TEST logo_hands_off_at_2000_in_header(void)
{
    logo_frame f = logo_frame_at(2000, false, IMG_W, IMG_H);
    ASSERT(f.done);
    ASSERT_EQ_FMT(19.3f, f.x, "%f");
    ASSERT_EQ_FMT(11.5f, f.y, "%f");
    ASSERT_EQ_FMT(100.0f, f.w, "%f");
    ASSERT_EQ_FMT(25.0f, f.h, "%f");
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.panel_alpha, EPS);
    logo_frame later = logo_frame_at(LOGO_TOTAL_MS, false, IMG_W, IMG_H);
    ASSERT(later.done);
    ASSERT_EQ_FMT(19.3f, later.x, "%f");
    ASSERT_EQ_FMT(100.0f, later.w, "%f");
    PASS();
}

TEST logo_phase_boundaries(void)
{
    ASSERT_EQ(600u, LOGO_INTRO_END_MS);
    ASSERT_EQ(1400u, LOGO_SHINE_END_MS);
    ASSERT_EQ(2000u, LOGO_HANDOFF_MS);
    ASSERT_EQ(2500u, LOGO_TOTAL_MS);
    ASSERT_IN_RANGE(285.3f, LOGO_FULL_W, EPS);
    ASSERT_IN_RANGE(333.3f, LOGO_PANEL_W, EPS);
    ASSERT_IN_RANGE(124.4f, LOGO_PANEL_H, EPS);
    PASS();
}

TEST logo_skip_jumps_to_end(void)
{
    logo_frame f = logo_frame_at(100, true, IMG_W, IMG_H);
    ASSERT(f.done);
    ASSERT_EQ_FMT(19.3f, f.x, "%f");
    ASSERT_EQ_FMT(11.5f, f.y, "%f");
    ASSERT_EQ_FMT(100.0f, f.w, "%f");
    ASSERT_EQ_FMT(25.0f, f.h, "%f");
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.panel_alpha, EPS);
    ASSERT_IN_RANGE(1.0f, f.shine, EPS);
    PASS();
}

TEST logo_guide_panels_slide_in_1400_to_2000(void)
{
    ASSERT_IN_RANGE(0.0f, logo_frame_at(0, false, IMG_W, IMG_H).panels, EPS);
    ASSERT_IN_RANGE(0.0f, logo_frame_at(1400, false, IMG_W, IMG_H).panels, EPS);
    ASSERT_IN_RANGE(0.875f, logo_frame_at(1700, false, IMG_W, IMG_H).panels, EPS);
    ASSERT_IN_RANGE(1.0f, logo_frame_at(2000, false, IMG_W, IMG_H).panels, EPS);
    ASSERT_IN_RANGE(1.0f, logo_frame_at(100, true, IMG_W, IMG_H).panels, EPS);
    ASSERT_IN_RANGE(40.0f, LOGO_PANEL_SLIDE_PX, EPS);
    PASS();
}

TEST logo_dots_cycle_every_300ms(void)
{
    ASSERT_EQ(0, logo_frame_at(0, false, IMG_W, IMG_H).dot);
    ASSERT_EQ(0, logo_frame_at(299, false, IMG_W, IMG_H).dot);
    ASSERT_EQ(1, logo_frame_at(300, false, IMG_W, IMG_H).dot);
    ASSERT_EQ(2, logo_frame_at(600, false, IMG_W, IMG_H).dot);
    ASSERT_EQ(0, logo_frame_at(900, false, IMG_W, IMG_H).dot);
    ASSERT_EQ(1, logo_frame_at(1300, false, IMG_W, IMG_H).dot);
    PASS();
}

SUITE(logoanim_suite)
{
    RUN_TEST(logo_header_rect_fits_header);
    RUN_TEST(logo_starts_small_and_clear);
    RUN_TEST(logo_scales_in_with_ease_out);
    RUN_TEST(logo_is_full_size_at_600);
    RUN_TEST(logo_shine_sweeps_600_to_1400);
    RUN_TEST(logo_moves_to_header_1400_to_2000);
    RUN_TEST(logo_hands_off_at_2000_in_header);
    RUN_TEST(logo_phase_boundaries);
    RUN_TEST(logo_skip_jumps_to_end);
    RUN_TEST(logo_guide_panels_slide_in_1400_to_2000);
    RUN_TEST(logo_dots_cycle_every_300ms);
}
