#include "greatest/greatest.h"
#include "osd_layout.h"

#define EPS 0.2f
#define WIDE 0.75f

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

static void centre(asp_rect r, float *x, float *y)
{
    *x = r.x + r.w / 2.0f;
    *y = r.y + r.h / 2.0f;
}

static bool within(asp_rect inner, asp_rect outer)
{
    return inner.x >= outer.x - EPS && inner.x + inner.w <= outer.x + outer.w + EPS &&
           inner.y >= outer.y - EPS && inner.y + inner.h <= outer.y + outer.h + EPS;
}

TEST layout_panel_sits_at_the_bottom(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(26.0f, l.panel.x, EPS);
    ASSERT_IN_RANGE(588.0f, l.panel.w, EPS);
    ASSERT_IN_RANGE(108.0f, l.panel.h, EPS);
    ASSERT_IN_RANGE(480.0f - 24.0f, l.panel.y + l.panel.h, EPS);
    PASS();
}

TEST layout_offset_moves_everything_down(void)
{
    osd_layout a = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    osd_layout b = osd_layout_for(50.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(a.panel.y + 50.0f, b.panel.y, EPS);
    ASSERT_IN_RANGE(a.name.y + 50.0f, b.name.y, EPS);
    ASSERT_IN_RANGE(a.bar.y + 50.0f, b.bar.y, EPS);
    ASSERT_IN_RANGE(a.countdown.y + 50.0f, b.countdown.y, EPS);
    ASSERT_IN_RANGE(a.name.x, b.name.x, EPS);
    ASSERT_IN_RANGE(a.vol_panel.y, b.vol_panel.y, EPS);
    PASS();
}

TEST layout_contents_stay_inside_the_panel(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    asp_rect all[] = { l.icon, l.name, l.number, l.line, l.start, l.bar, l.end, l.divider, l.next_icon, l.next_label, l.next_title, l.next_at, l.countdown };
    for (size_t i = 0; i < sizeof all / sizeof all[0]; i++) ASSERT(within(all[i], l.panel));
    PASS();
}

TEST layout_matches_the_deck(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    float px = l.panel.x, py = l.panel.y;
    ASSERT_IN_RANGE(px + 14.0f, l.icon.x, EPS);
    ASSERT_IN_RANGE(py + 30.0f, l.icon.y, EPS);
    ASSERT_IN_RANGE(48.0f, l.icon.h, EPS);
    ASSERT_IN_RANGE(36.0f, l.icon.w, EPS);
    ASSERT_IN_RANGE(px + 60.7f, l.name.x, EPS);
    ASSERT_IN_RANGE(py + 22.3f, l.name.y, EPS);
    ASSERT_IN_RANGE(px + 60.7f, l.line.x, EPS);
    ASSERT_IN_RANGE(py + 47.2f, l.line.y, EPS);
    ASSERT_IN_RANGE(py + 70.8f, l.start.y, EPS);
    ASSERT_IN_RANGE(px + 385.3f, l.end.x + l.end.w, EPS);
    ASSERT_IN_RANGE(px + 396.0f, l.divider.x, EPS);
    ASSERT_IN_RANGE(px + 407.3f, l.next_icon.x, EPS);
    ASSERT_IN_RANGE(40.0f, l.next_icon.h, EPS);
    ASSERT_IN_RANGE(px + 445.3f, l.next_label.x, EPS);
    ASSERT_IN_RANGE(py + 20.4f, l.next_label.y, EPS);
    ASSERT_IN_RANGE(py + 38.1f, l.next_title.y, EPS);
    ASSERT_IN_RANGE(px + 515.7f, l.next_title.x + l.next_title.w, EPS);
    ASSERT(l.next_title.x + l.next_title.w <= l.countdown.x);
    ASSERT_IN_RANGE(py + 73.7f, l.next_at.y, EPS);
    ASSERT_IN_RANGE(px + 574.0f, l.countdown.x + l.countdown.w, EPS);
    ASSERT_IN_RANGE(py + 40.0f, l.countdown.y, EPS);
    PASS();
}

TEST layout_number_pill_follows_the_name(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(l.panel.x + 60.7f + 100.0f + 5.3f, l.number.x, EPS);
    ASSERT_IN_RANGE(l.panel.y + 24.3f, l.number.y, EPS);
    ASSERT_IN_RANGE(16.0f, l.number.h, EPS);
    osd_layout wide = osd_layout_for(0.0f, WIDE, 150.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(l.number.x + 50.0f, wide.number.x, EPS);
    PASS();
}

TEST layout_bar_follows_the_start_time(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(l.start.x + 30.0f + 5.9f, l.bar.x, EPS);
    ASSERT_IN_RANGE(l.panel.x + 352.0f, l.bar.x + l.bar.w, EPS);
    ASSERT_IN_RANGE(l.panel.y + 74.3f, l.bar.y, EPS);
    ASSERT_IN_RANGE(8.0f, l.bar.h, EPS);
    osd_layout wide = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 40.0f, 50.0f);
    ASSERT_IN_RANGE(l.bar.x + 10.0f, wide.bar.x, EPS);
    ASSERT_IN_RANGE(l.bar.w - 10.0f, wide.bar.w, EPS);
    PASS();
}

TEST hit_only_volume_pills_and_only_while_visible(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    float x, y;
    centre(l.vol_down, &x, &y);
    ASSERT_EQ(OSD_BTN_VOL_DOWN, osd_hit(&l, x, y, true));
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, x, y, false));
    centre(l.vol_up, &x, &y);
    ASSERT_EQ(OSD_BTN_VOL_UP, osd_hit(&l, x, y, true));
    centre(l.name, &x, &y);
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, x, y, true));
    centre(l.vol_bar, &x, &y);
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, x, y, true));
    ASSERT_EQ(OSD_BTN_NONE, osd_hit(&l, 5.0f, 5.0f, true));
    PASS();
}

TEST segments_tile_the_volume_bar(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(185.3f, l.vol_bar.w, EPS);
    ASSERT_IN_RANGE(16.0f, l.vol_bar.h, EPS);
    asp_rect first = osd_segment(l.vol_bar, 0);
    asp_rect last = osd_segment(l.vol_bar, OSD_VOLUME_SEGMENTS - 1);
    ASSERT_IN_RANGE(l.vol_bar.x, first.x, EPS);
    ASSERT_IN_RANGE(l.vol_bar.x + l.vol_bar.w, last.x + last.w, EPS);
    asp_rect second = osd_segment(l.vol_bar, 1);
    ASSERT_IN_RANGE(first.x + first.w + 3.0f, second.x, EPS);
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
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(190.0f, l.vol_panel.x, EPS);
    ASSERT_IN_RANGE(412.0f, l.vol_panel.y, EPS);
    ASSERT_IN_RANGE(260.0f, l.vol_panel.w, EPS);
    ASSERT_IN_RANGE(44.0f, l.vol_panel.h, EPS);
    ASSERT_IN_RANGE(387.0f, l.vol_label.y, EPS);
    ASSERT_IN_RANGE(320.0f, l.vol_label.x + l.vol_label.w / 2.0f, EPS);
    ASSERT_IN_RANGE(195.3f, l.vol_down.x, EPS);
    ASSERT_IN_RANGE(418.0f, l.vol_down.y, EPS);
    ASSERT_IN_RANGE(24.0f, l.vol_down.w, EPS);
    ASSERT_IN_RANGE(32.0f, l.vol_down.h, EPS);
    ASSERT_IN_RANGE(420.7f, l.vol_up.x, EPS);
    ASSERT_IN_RANGE(227.3f, l.vol_bar.x, EPS);
    ASSERT_IN_RANGE(426.0f, l.vol_bar.y, EPS);
    ASSERT(within(l.vol_down, l.vol_panel) && within(l.vol_up, l.vol_panel) && within(l.vol_bar, l.vol_panel));
    PASS();
}

TEST scrim_covers_the_lower_half(void)
{
    asp_rect s = osd_scrim();
    ASSERT_IN_RANGE(0.0f, s.x, EPS);
    ASSERT_IN_RANGE(248.9f, s.y, EPS);
    ASSERT_IN_RANGE(640.0f, s.w, EPS);
    ASSERT_IN_RANGE(480.0f, s.y + s.h, EPS);
    PASS();
}

TEST chip_sits_in_the_top_right_corner(void)
{
    osd_chip c = osd_chip_layout(18.0f, 60.0f, WIDE);
    ASSERT_IN_RANGE(628.0f, c.chip.x + c.chip.w, EPS);
    ASSERT_IN_RANGE(16.0f, c.chip.y, EPS);
    ASSERT_IN_RANGE(57.8f, c.chip.h, EPS);
    ASSERT_IN_RANGE(81.3f, c.chip.w, EPS);
    ASSERT_IN_RANGE(c.chip.x + 7.3f, c.tile.x, EPS);
    ASSERT_IN_RANGE(c.chip.y + 8.9f, c.tile.y, EPS);
    ASSERT_IN_RANGE(30.0f, c.tile.w, EPS);
    ASSERT_IN_RANGE(40.0f, c.tile.h, EPS);
    ASSERT_IN_RANGE(c.tile.x + c.tile.w + 4.7f, c.icon.x, EPS);
    ASSERT_IN_RANGE(40.0f, c.icon.h, EPS);
    ASSERT_IN_RANGE(30.0f, c.icon.w, EPS);
    PASS();
}

TEST chip_grows_with_the_number(void)
{
    osd_chip narrow = osd_chip_layout(18.0f, 60.0f, WIDE);
    osd_chip wide = osd_chip_layout(60.0f, 60.0f, WIDE);
    ASSERT_IN_RANGE(72.0f, wide.tile.w, EPS);
    ASSERT_IN_RANGE(narrow.chip.w + 42.0f, wide.chip.w, EPS);
    ASSERT_IN_RANGE(narrow.chip.x + narrow.chip.w, wide.chip.x + wide.chip.w, EPS);
    PASS();
}

TEST chip_4_3_icon_clears_tile(void)
{
    osd_chip c = osd_chip_layout(18.0f, 60.0f, 1.0f);
    ASSERT_IN_RANGE(40.0f, c.icon.w, EPS);
    ASSERT(c.icon.x >= c.tile.x + c.tile.w);
    ASSERT(c.icon.x + c.icon.w <= c.chip.x + c.chip.w);
    PASS();
}

TEST chip_name_pill_hangs_right_aligned(void)
{
    osd_chip c = osd_chip_layout(18.0f, 60.0f, WIDE);
    ASSERT_IN_RANGE(c.chip.x + c.chip.w, c.name_pill.x + c.name_pill.w, EPS);
    ASSERT_IN_RANGE(84.4f, c.name_pill.y, EPS);
    ASSERT_IN_RANGE(28.4f, c.name_pill.h, EPS);
    ASSERT_IN_RANGE(88.0f, c.name_pill.w, EPS);
    osd_chip tiny = osd_chip_layout(18.0f, 10.0f, WIDE);
    ASSERT_IN_RANGE(60.0f, tiny.name_pill.w, EPS);
    PASS();
}

TEST inside_helper_sanity(void)
{
    asp_rect r = { 10.0f, 10.0f, 10.0f, 10.0f };
    ASSERT(inside(r, 15.0f, 15.0f));
    ASSERT(!inside(r, 25.0f, 15.0f));
    PASS();
}

TEST layout_title_column_yields_to_a_wide_countdown(void)
{
    osd_layout narrow = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(70.4f, narrow.next_title.w, EPS);
    osd_layout wide = osd_layout_for(0.0f, WIDE, 100.0f, 20.0f, 30.0f, 90.0f);
    ASSERT_IN_RANGE(wide.panel.x + 574.0f - 90.0f, wide.countdown.x, EPS);
    ASSERT_IN_RANGE(90.0f, wide.countdown.w, EPS);
    ASSERT(wide.next_title.x + wide.next_title.w <= wide.countdown.x - 6.0f + EPS);
    ASSERT(wide.next_title.w < narrow.next_title.w);
    ASSERT_IN_RANGE(wide.next_title.x, narrow.next_title.x, EPS);
    PASS();
}

TEST layout_number_pill_uses_its_width(void)
{
    osd_layout l = osd_layout_for(0.0f, WIDE, 100.0f, 34.0f, 30.0f, 50.0f);
    ASSERT_IN_RANGE(34.0f, l.number.w, EPS);
    ASSERT(l.number.x + l.number.w <= l.panel.x + l.panel.w);
    PASS();
}

SUITE(osd_layout_suite)
{
    RUN_TEST(layout_title_column_yields_to_a_wide_countdown);
    RUN_TEST(layout_number_pill_uses_its_width);
    RUN_TEST(layout_panel_sits_at_the_bottom);
    RUN_TEST(layout_offset_moves_everything_down);
    RUN_TEST(layout_contents_stay_inside_the_panel);
    RUN_TEST(layout_matches_the_deck);
    RUN_TEST(layout_number_pill_follows_the_name);
    RUN_TEST(layout_bar_follows_the_start_time);
    RUN_TEST(hit_only_volume_pills_and_only_while_visible);
    RUN_TEST(segments_tile_the_volume_bar);
    RUN_TEST(segments_lit_counts_tens);
    RUN_TEST(standalone_volume_is_centred_above_the_bottom);
    RUN_TEST(scrim_covers_the_lower_half);
    RUN_TEST(chip_sits_in_the_top_right_corner);
    RUN_TEST(chip_grows_with_the_number);
    RUN_TEST(chip_4_3_icon_clears_tile);
    RUN_TEST(chip_name_pill_hangs_right_aligned);
    RUN_TEST(inside_helper_sanity);
}
