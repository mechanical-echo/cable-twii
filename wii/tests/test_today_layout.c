#include "greatest/greatest.h"
#include "today_layout.h"

#define EPS 0.01f

static enum greatest_test_res rect_is(asp_rect r, float x, float y, float w, float h)
{
    ASSERT_IN_RANGE(x, r.x, EPS);
    ASSERT_IN_RANGE(y, r.y, EPS);
    ASSERT_IN_RANGE(w, r.w, EPS);
    ASSERT_IN_RANGE(h, r.h, EPS);
    PASS();
}

TEST card_matches_the_mockup(void)
{
    today_layout l = today_layout_for(1.0f);
    CHECK_CALL(rect_is(l.card, 30.0f, 22.0f, 580.0f, 436.0f));
    CHECK_CALL(rect_is(l.icon, 44.0f, 34.0f, 36.0f, 36.0f));
    CHECK_CALL(rect_is(l.name, 89.0f, 34.0f, 425.0f, 18.0f));
    CHECK_CALL(rect_is(l.date, 89.0f, 57.0f, 425.0f, 12.0f));
    CHECK_CALL(rect_is(l.tag, 594.0f, 39.0f, 0.0f, 20.0f));
    CHECK_CALL(rect_is(l.rule_top, 42.0f, 80.0f, 556.0f, 1.0f));
    CHECK_CALL(rect_is(l.list, 38.0f, 86.0f, 550.0f, 328.0f));
    CHECK_CALL(rect_is(l.scroll, 597.7f, 90.0f, 3.0f, 320.0f));
    CHECK_CALL(rect_is(l.rule_bottom, 42.0f, 418.0f, 556.0f, 1.0f));
    CHECK_CALL(rect_is(l.hints, 48.0f, 428.0f, 0.0f, TL_HINT_DISC));
    CHECK_CALL(rect_is(l.skip_note, 592.0f, 432.0f, 0.0f, 12.0f));
    today_layout n = today_layout_for(0.75f);
    CHECK_CALL(rect_is(n.icon, 44.0f, 34.0f, 27.0f, 36.0f));
    CHECK_CALL(rect_is(n.name, 80.0f, 34.0f, 434.0f, 18.0f));
    CHECK_CALL(rect_is(n.list, 38.0f, 86.0f, 550.0f, 328.0f));
    PASS();
}

TEST list_sits_between_the_rules(void)
{
    today_layout l = today_layout_for(1.0f);
    ASSERT(l.rule_top.y > l.icon.y + l.icon.h);
    ASSERT(l.rule_top.y < l.list.y);
    ASSERT(l.list.y + l.list.h < l.rule_bottom.y);
    ASSERT(l.scroll.x > l.list.x + l.list.w);
    ASSERT(l.scroll.x + l.scroll.w < l.card.x + l.card.w);
    ASSERT(l.scroll.y >= l.list.y && l.scroll.y + l.scroll.h <= l.list.y + l.list.h);
    PASS();
}

static enum greatest_test_res columns_fit(float px)
{
    asp_rect row = { 38.0f, 90.0f, 550.0f, 32.0f };
    today_cols c = today_cols_for(row, 96.0f, 46.0f, px);
    ASSERT(c.time_x > row.x);
    ASSERT(c.time_x + 96.0f < c.icon_x);
    ASSERT(c.icon_x + c.icon_w < c.show_x);
    ASSERT(c.show_x + c.show_w < c.code_x);
    ASSERT(c.code_x + 46.0f < c.title_x);
    ASSERT(c.title_x + c.title_w < row.x + row.w);
    ASSERT(c.title_w > 100.0f);
    PASS();
}

TEST columns_fit_a_marked_time(void)
{
    CHECK_CALL(columns_fit(1.0f));
    CHECK_CALL(columns_fit(0.75f));
    asp_rect row = { 38.0f, 90.0f, 550.0f, 32.0f };
    today_cols c = today_cols_for(row, 96.0f, 46.0f, 1.0f);
    ASSERT_IN_RANGE(52.0f, c.time_x, EPS);
    ASSERT_IN_RANGE(160.0f, c.icon_x, EPS);
    ASSERT_IN_RANGE(22.0f, c.icon_w, EPS);
    ASSERT_IN_RANGE(186.0f, c.show_x, EPS);
    ASSERT_IN_RANGE(110.0f, c.show_w, EPS);
    ASSERT_IN_RANGE(304.0f, c.code_x, EPS);
    ASSERT_IN_RANGE(360.0f, c.title_x, EPS);
    ASSERT_IN_RANGE(218.0f, c.title_w, EPS);
    PASS();
}

TEST full_hint_sits_above_the_panel(void)
{
    asp_rect panel = { 26.0f, 348.0f, 588.0f, 108.0f };
    CHECK_CALL(rect_is(today_hint_full(panel, 80.0f), 534.0f, 313.3f, 80.0f, 26.7f));
    PASS();
}

TEST guide_hint_follows_watch(void)
{
    asp_rect preview = { 307.3f, 58.7f, 313.3f, 235.1f };
    CHECK_CALL(rect_is(today_hint_guide(preview, 70.0f, 1.0f), 400.6f, 255.0f, 70.0f, 26.7f));
    CHECK_CALL(rect_is(today_hint_guide(preview, 70.0f, 0.75f), 399.35f, 255.0f, 70.0f, 26.7f));
    PASS();
}

TEST hint_w_counts_disc_and_label(void)
{
    ASSERT_IN_RANGE(65.3f, today_hint_w(30.0f, 1.0f), EPS);
    ASSERT_IN_RANGE(58.15f, today_hint_w(30.0f, 0.75f), EPS);
    PASS();
}

SUITE(today_layout_suite)
{
    RUN_TEST(card_matches_the_mockup);
    RUN_TEST(list_sits_between_the_rules);
    RUN_TEST(columns_fit_a_marked_time);
    RUN_TEST(full_hint_sits_above_the_panel);
    RUN_TEST(guide_hint_follows_watch);
    RUN_TEST(hint_w_counts_disc_and_label);
}
