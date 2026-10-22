#include "greatest/greatest.h"
#include "osd_layout.h"

#define EPS 0.01f

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

static void centre(asp_rect r, float *x, float *y)
{
    *x = r.x + r.w / 2.0f;
    *y = r.y + r.h / 2.0f;
}

TEST layout_panel_sits_at_the_bottom(void)
{
    osd_layout l = osd_layout_for(0.0f);
    ASSERT_IN_RANGE(26.0f, l.panel.x, EPS);
    ASSERT_IN_RANGE(588.0f, l.panel.w, EPS);
    ASSERT_IN_RANGE(108.0f, l.panel.h, EPS);
    ASSERT_IN_RANGE(480.0f - 24.0f, l.panel.y + l.panel.h, EPS);
    PASS();
}

TEST layout_offset_moves_everything_down(void)
{
    osd_layout a = osd_layout_for(0.0f);
    osd_layout b = osd_layout_for(50.0f);
    ASSERT_IN_RANGE(a.panel.y + 50.0f, b.panel.y, EPS);
    ASSERT_IN_RANGE(a.next.y + 50.0f, b.next.y, EPS);
    ASSERT_IN_RANGE(a.vol_bar.y + 50.0f, b.vol_bar.y, EPS);
    ASSERT_IN_RANGE(a.next.x, b.next.x, EPS);
    ASSERT_IN_RANGE(a.bar.y + 50.0f, b.bar.y, EPS);
    PASS();
}

TEST layout_contents_stay_inside_the_panel(void)
{
    osd_layout l = osd_layout_for(0.0f);
    asp_rect all[] = { l.name, l.icon, l.line, l.times, l.bar, l.prev, l.next, l.guide, l.vol_down, l.vol_bar, l.vol_up };
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) {
        ASSERT(all[i].x >= l.panel.x && all[i].x + all[i].w <= l.panel.x + l.panel.w);
        ASSERT(all[i].y >= l.panel.y && all[i].y + all[i].h <= l.panel.y + l.panel.h);
    }
    PASS();
}

TEST layout_buttons_do_not_overlap(void)
{
    osd_layout l = osd_layout_for(0.0f);
    asp_rect all[] = { l.prev, l.next, l.guide, l.vol_down, l.vol_bar, l.vol_up };
    size_t n = sizeof all / sizeof all[0];
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            bool apart = all[i].x + all[i].w <= all[j].x || all[j].x + all[j].w <= all[i].x ||
                         all[i].y + all[i].h <= all[j].y || all[j].y + all[j].h <= all[i].y;
            ASSERT(apart);
        }
    }
    PASS();
}

TEST hit_finds_each_button_at_its_centre(void)
{
    osd_layout l = osd_layout_for(0.0f);
    float x, y;
    centre(l.prev, &x, &y);
    ASSERT_EQ(OSD_BTN_PREV, osd_hit(&l, x, y));
    centre(l.next, &x, &y);
    ASSERT_EQ(OSD_BTN_NEXT, osd_hit(&l, x, y));
    centre(l.guide, &x, &y);
    ASSERT_EQ(OSD_BTN_GUIDE, osd_hit(&l, x, y));
    centre(l.vol_down, &x, &y);
    ASSERT_EQ(OSD_BTN_VOL_DOWN, osd_hit(&l, x, y));
    centre(l.vol_up, &x, &y);
    ASSERT_EQ(OSD_BTN_VOL_UP, osd_hit(&l, x, y));
    PASS();
}

TEST hit_misses_gaps_and_outside(void)
{
    osd_layout l = osd_layout_for(0.0f);
    float gap_x = (l.prev.x + l.prev.w + l.next.x) / 2.0f;
    ASSERT(!inside(l.prev, gap_x, l.prev.y + 1.0f));
    ASSERT(!inside(l.next, gap_x, l.next.y + 1.0f));
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, gap_x, l.prev.y + 1.0f));
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, 5.0f, 5.0f));
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, l.name.x + 4.0f, l.name.y + 4.0f));
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, l.vol_bar.x + 4.0f, l.vol_bar.y + 4.0f));
    PASS();
}

TEST hit_follows_the_slide_offset(void)
{
    osd_layout rest = osd_layout_for(0.0f);
    osd_layout low = osd_layout_for(80.0f);
    float x, y;
    centre(rest.next, &x, &y);
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&low, x, y));
    ASSERT_EQ(OSD_BTN_NEXT, osd_hit(&low, x, y + 80.0f));
    PASS();
}

TEST segments_tile_the_volume_bar(void)
{
    osd_layout l = osd_layout_for(0.0f);
    asp_rect first = osd_segment(l.vol_bar, 0);
    asp_rect last = osd_segment(l.vol_bar, OSD_VOLUME_SEGMENTS - 1);
    ASSERT_IN_RANGE(l.vol_bar.x, first.x, EPS);
    ASSERT_IN_RANGE(l.vol_bar.x + l.vol_bar.w, last.x + last.w, EPS);
    ASSERT_IN_RANGE(l.vol_bar.h, first.h, EPS);
    asp_rect second = osd_segment(l.vol_bar, 1);
    ASSERT(second.x > first.x + first.w);
    PASS();
}

TEST segments_lit_counts_tens(void)
{
    ASSERT_EQ(10, osd_segments_lit(100));
    ASSERT_EQ(5, osd_segments_lit(50));
    ASSERT_EQ(0, osd_segments_lit(0));
    ASSERT_EQ(1, osd_segments_lit(1));
    ASSERT_EQ(10, osd_segments_lit(250));
    ASSERT_EQ(0, osd_segments_lit(-5));
    PASS();
}

TEST standalone_volume_is_centred_above_the_bottom(void)
{
    osd_layout l = osd_layout_for(0.0f);
    ASSERT_IN_RANGE(320.0f, l.vol_panel.x + l.vol_panel.w / 2.0f, EPS);
    ASSERT_IN_RANGE(480.0f - 24.0f, l.vol_panel.y + l.vol_panel.h, EPS);
    ASSERT(l.vol_panel_bar.x >= l.vol_panel.x && l.vol_panel_bar.x + l.vol_panel_bar.w <= l.vol_panel.x + l.vol_panel.w);
    ASSERT(l.vol_panel_bar.y >= l.vol_panel.y && l.vol_panel_bar.y + l.vol_panel_bar.h <= l.vol_panel.y + l.vol_panel.h);
    PASS();
}

TEST chip_sits_in_the_top_right_corner(void)
{
    osd_chip c = osd_chip_layout(40.0f);
    ASSERT_IN_RANGE(640.0f - 16.0f, c.chip.x + c.chip.w, EPS);
    ASSERT_IN_RANGE(16.0f, c.chip.y, EPS);
    ASSERT_IN_RANGE(56.0f, c.chip.h, EPS);
    PASS();
}

TEST chip_icon_is_40px_and_centred(void)
{
    osd_chip c = osd_chip_layout(40.0f);
    ASSERT_IN_RANGE(40.0f, c.icon.w, EPS);
    ASSERT_IN_RANGE(40.0f, c.icon.h, EPS);
    ASSERT_IN_RANGE(c.chip.y + (c.chip.h - 40.0f) / 2.0f, c.icon.y, EPS);
    ASSERT(c.icon.x > c.chip.x);
    PASS();
}

TEST chip_number_follows_the_icon(void)
{
    osd_chip c = osd_chip_layout(40.0f);
    ASSERT(c.number.x >= c.icon.x + c.icon.w);
    ASSERT_IN_RANGE(40.0f, c.number.w, EPS);
    ASSERT(c.number.x + c.number.w <= c.chip.x + c.chip.w);
    ASSERT(c.number.y >= c.chip.y && c.number.y + c.number.h <= c.chip.y + c.chip.h);
    PASS();
}

TEST chip_grows_with_the_number(void)
{
    osd_chip narrow = osd_chip_layout(20.0f);
    osd_chip wide = osd_chip_layout(80.0f);
    ASSERT_IN_RANGE(narrow.chip.w + 60.0f, wide.chip.w, EPS);
    ASSERT_IN_RANGE(narrow.chip.x + narrow.chip.w, wide.chip.x + wide.chip.w, EPS);
    PASS();
}

TEST chip_caps_long_numbers(void)
{
    osd_chip c = osd_chip_layout(1000.0f);
    ASSERT_IN_RANGE(OSD_CHIP_NUMBER_MAX_W, c.number.w, EPS);
    ASSERT(c.chip.x >= 320.0f);
    PASS();
}

SUITE(osd_layout_suite)
{
    RUN_TEST(layout_panel_sits_at_the_bottom);
    RUN_TEST(layout_offset_moves_everything_down);
    RUN_TEST(layout_contents_stay_inside_the_panel);
    RUN_TEST(layout_buttons_do_not_overlap);
    RUN_TEST(hit_finds_each_button_at_its_centre);
    RUN_TEST(hit_misses_gaps_and_outside);
    RUN_TEST(hit_follows_the_slide_offset);
    RUN_TEST(segments_tile_the_volume_bar);
    RUN_TEST(segments_lit_counts_tens);
    RUN_TEST(standalone_volume_is_centred_above_the_bottom);
    RUN_TEST(chip_sits_in_the_top_right_corner);
    RUN_TEST(chip_icon_is_40px_and_centred);
    RUN_TEST(chip_number_follows_the_icon);
    RUN_TEST(chip_grows_with_the_number);
    RUN_TEST(chip_caps_long_numbers);
}
