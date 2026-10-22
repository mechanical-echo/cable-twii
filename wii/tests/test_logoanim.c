#include "greatest/greatest.h"
#include "logoanim.h"

#define EPS 0.001f

TEST logo_header_rect_fits_header(void)
{
    float x, y, w, h;
    logo_header_rect_for(256.0f, 64.0f, &x, &y, &w, &h);
    ASSERT_IN_RANGE(19.3f, x, EPS);
    ASSERT_IN_RANGE(11.5f, y, EPS);
    ASSERT_IN_RANGE(100.0f, w, EPS);
    ASSERT_IN_RANGE(25.0f, h, EPS);
    PASS();
}

TEST logo_starts_small_and_clear(void)
{
    logo_frame f = logo_frame_at(0, false, 256.0f, 64.0f);
    ASSERT_IN_RANGE(0.6f, f.scale, EPS);
    ASSERT_IN_RANGE(0.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.shine, EPS);
    ASSERT_IN_RANGE(153.6f, f.w, EPS);
    ASSERT_IN_RANGE(38.4f, f.h, EPS);
    ASSERT_IN_RANGE(320.0f - 76.8f, f.x, EPS);
    ASSERT_IN_RANGE(240.0f - 19.2f, f.y, EPS);
    ASSERT_FALSE(f.done);
    PASS();
}

TEST logo_scales_in_with_ease_out(void)
{
    logo_frame f = logo_frame_at(300, false, 256.0f, 64.0f);
    ASSERT_IN_RANGE(0.95f, f.scale, EPS);
    ASSERT_IN_RANGE(0.875f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.shine, EPS);
    ASSERT_IN_RANGE(320.0f - 121.6f, f.x, EPS);
    PASS();
}

TEST logo_is_full_size_at_600(void)
{
    logo_frame f = logo_frame_at(600, false, 256.0f, 64.0f);
    ASSERT_IN_RANGE(1.0f, f.scale, EPS);
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(0.0f, f.shine, EPS);
    ASSERT_IN_RANGE(192.0f, f.x, EPS);
    ASSERT_IN_RANGE(208.0f, f.y, EPS);
    ASSERT_IN_RANGE(256.0f, f.w, EPS);
    ASSERT_IN_RANGE(64.0f, f.h, EPS);
    PASS();
}

TEST logo_shine_sweeps_600_to_1400(void)
{
    logo_frame mid = logo_frame_at(1000, false, 256.0f, 64.0f);
    ASSERT_IN_RANGE(0.875f, mid.shine, EPS);
    ASSERT_IN_RANGE(1.0f, mid.scale, EPS);
    ASSERT_IN_RANGE(192.0f, mid.x, EPS);
    logo_frame end = logo_frame_at(1400, false, 256.0f, 64.0f);
    ASSERT_IN_RANGE(1.0f, end.shine, EPS);
    ASSERT_IN_RANGE(192.0f, end.x, EPS);
    ASSERT_IN_RANGE(208.0f, end.y, EPS);
    ASSERT_IN_RANGE(256.0f, end.w, EPS);
    ASSERT_FALSE(end.done);
    PASS();
}

TEST logo_moves_to_header_1400_to_2000(void)
{
    logo_frame f = logo_frame_at(1700, false, 256.0f, 64.0f);
    ASSERT_IN_RANGE(192.0f + (19.3f - 192.0f) * 0.875f, f.x, EPS);
    ASSERT_IN_RANGE(208.0f + (11.5f - 208.0f) * 0.875f, f.y, EPS);
    ASSERT_IN_RANGE(256.0f + (100.0f - 256.0f) * 0.875f, f.w, EPS);
    ASSERT_IN_RANGE(64.0f + (25.0f - 64.0f) * 0.875f, f.h, EPS);
    ASSERT_IN_RANGE(f.w / 256.0f, f.scale, EPS);
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(1.0f, f.shine, EPS);
    ASSERT_FALSE(f.done);
    ASSERT_FALSE(logo_frame_at(1999, false, 256.0f, 64.0f).done);
    PASS();
}

TEST logo_hands_off_at_2000_in_header(void)
{
    logo_frame f = logo_frame_at(2000, false, 256.0f, 64.0f);
    ASSERT(f.done);
    ASSERT_EQ_FMT(19.3f, f.x, "%f");
    ASSERT_EQ_FMT(11.5f, f.y, "%f");
    ASSERT_EQ_FMT(100.0f, f.w, "%f");
    ASSERT_EQ_FMT(25.0f, f.h, "%f");
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    logo_frame later = logo_frame_at(LOGO_TOTAL_MS, false, 256.0f, 64.0f);
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
    PASS();
}

TEST logo_skip_jumps_to_end(void)
{
    logo_frame f = logo_frame_at(100, true, 256.0f, 64.0f);
    ASSERT(f.done);
    ASSERT_EQ_FMT(19.3f, f.x, "%f");
    ASSERT_EQ_FMT(11.5f, f.y, "%f");
    ASSERT_EQ_FMT(100.0f, f.w, "%f");
    ASSERT_EQ_FMT(25.0f, f.h, "%f");
    ASSERT_IN_RANGE(1.0f, f.alpha, EPS);
    ASSERT_IN_RANGE(1.0f, f.shine, EPS);
    PASS();
}

TEST logo_guide_panels_slide_in_1400_to_2000(void)
{
    ASSERT_IN_RANGE(0.0f, logo_frame_at(0, false, 256.0f, 64.0f).panels, EPS);
    ASSERT_IN_RANGE(0.0f, logo_frame_at(1400, false, 256.0f, 64.0f).panels, EPS);
    ASSERT_IN_RANGE(0.875f, logo_frame_at(1700, false, 256.0f, 64.0f).panels, EPS);
    ASSERT_IN_RANGE(1.0f, logo_frame_at(2000, false, 256.0f, 64.0f).panels, EPS);
    ASSERT_IN_RANGE(1.0f, logo_frame_at(100, true, 256.0f, 64.0f).panels, EPS);
    ASSERT_IN_RANGE(40.0f, LOGO_PANEL_SLIDE_PX, EPS);
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
}
