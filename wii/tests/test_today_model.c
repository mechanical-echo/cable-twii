#include <string.h>
#include "greatest/greatest.h"
#include "today_model.h"

#define EPS 0.01f

static ctw_airing items[6];
static ctw_lineup lineup;

static void make_lineup(void)
{
    static const ctw_air_kind kinds[] = { CTW_AIR_EPISODE, CTW_AIR_AD, CTW_AIR_AD, CTW_AIR_EPISODE, CTW_AIR_AD, CTW_AIR_UPNEXT };
    static const uint32_t durs[] = { 600000u, 45000u, 45000u, 600000u, 60000u, 300000u };
    memset(items, 0, sizeof items);
    memset(&lineup, 0, sizeof lineup);
    int64_t at = 0;
    for (size_t i = 0; i < 6; i++) {
        items[i].start_ms = at;
        items[i].duration_ms = durs[i];
        items[i].kind = kinds[i];
        at += durs[i];
    }
    lineup.items = items;
    lineup.count = 6;
    lineup.cap = 6;
}

TEST build_merges_ads(void)
{
    make_lineup();
    td_row rows[6];
    ASSERT_EQ(5u, td_build(&lineup, rows, 6));
    ASSERT_EQ(TD_EPISODE, rows[0].kind);
    ASSERT_EQ((size_t)&items[0], (size_t)rows[0].first);
    ASSERT_EQ(1u, rows[0].count);
    ASSERT_EQ(TD_ADS, rows[1].kind);
    ASSERT_EQ((size_t)&items[1], (size_t)rows[1].first);
    ASSERT_EQ(2u, rows[1].count);
    ASSERT_EQ(90000u, rows[1].total_ms);
    ASSERT_EQ(items[1].start_ms, rows[1].start_ms);
    ASSERT_EQ(items[1].start_ms + 90000, rows[1].end_ms);
    ASSERT_EQ(TD_EPISODE, rows[2].kind);
    ASSERT_EQ(TD_ADS, rows[3].kind);
    ASSERT_EQ(1u, rows[3].count);
    ASSERT_EQ(60000u, rows[3].total_ms);
    ASSERT_EQ(TD_UPNEXT, rows[4].kind);
    ASSERT_EQ(items[5].start_ms + 300000, rows[4].end_ms);
    PASS();
}

TEST build_respects_cap(void)
{
    make_lineup();
    td_row rows[2];
    ASSERT_EQ(2u, td_build(&lineup, rows, 2));
    ASSERT_EQ(2u, rows[1].count);
    PASS();
}

TEST build_empty_is_zero(void)
{
    td_row rows[2];
    ctw_lineup empty;
    memset(&empty, 0, sizeof empty);
    ASSERT_EQ(0u, td_build(NULL, rows, 2));
    ASSERT_EQ(0u, td_build(&empty, rows, 2));
    PASS();
}

TEST row_of_finds_merged_ad(void)
{
    make_lineup();
    td_row rows[6];
    size_t n = td_build(&lineup, rows, 6);
    ASSERT_EQ(1, td_row_of(rows, n, &items[2]));
    ASSERT_EQ(0, td_row_of(rows, n, &items[0]));
    ctw_airing other;
    memset(&other, 0, sizeof other);
    ASSERT_EQ(-1, td_row_of(rows, n, &other));
    ASSERT_EQ(-1, td_row_of(rows, n, NULL));
    PASS();
}

TEST open_scroll_puts_now_third(void)
{
    ASSERT_IN_RANGE(td_row_y(8) - TD_PAD, td_open_scroll(10, 50, 314.0f), EPS);
    ASSERT_IN_RANGE(272.0f, td_open_scroll(10, 50, 314.0f), EPS);
    ASSERT_IN_RANGE(0.0f, td_open_scroll(1, 50, 314.0f), EPS);
    ASSERT_IN_RANGE(td_content_h(50) - 314.0f, td_open_scroll(49, 50, 314.0f), EPS);
    ASSERT_IN_RANGE(0.0f, td_open_scroll(-1, 50, 314.0f), EPS);
    ASSERT_IN_RANGE(0.0f, td_open_scroll(3, 4, 314.0f), EPS);
    PASS();
}

TEST scroll_to_keeps_row_in_view(void)
{
    ASSERT_IN_RANGE(td_row_y(20) + TD_ROW_H - 314.0f, td_scroll_to(20, 0.0f, 314.0f, 50), EPS);
    ASSERT_IN_RANGE(0.0f, td_scroll_to(0, 300.0f, 314.0f, 50), EPS);
    ASSERT_IN_RANGE(100.0f, td_scroll_to(5, 100.0f, 314.0f, 50), EPS);
    PASS();
}

TEST hit_maps_rows_and_gaps(void)
{
    ASSERT_EQ(0, td_hit(0.0f, TD_PAD + 1.0f, 50));
    ASSERT_EQ(-1, td_hit(0.0f, TD_PAD + TD_ROW_H + 1.0f, 50));
    ASSERT_EQ(-1, td_hit(0.0f, td_content_h(50) + 10.0f, 50));
    ASSERT_EQ(-1, td_hit(0.0f, 1.0f, 50));
    ASSERT_EQ(1, td_hit(34.0f, TD_PAD + 1.0f, 50));
    PASS();
}

TEST content_h_counts_rows(void)
{
    ASSERT_IN_RANGE(0.0f, td_content_h(0), EPS);
    ASSERT_IN_RANGE(1706.0f, td_content_h(50), EPS);
    PASS();
}

SUITE(today_model_suite)
{
    RUN_TEST(build_merges_ads);
    RUN_TEST(build_respects_cap);
    RUN_TEST(build_empty_is_zero);
    RUN_TEST(row_of_finds_merged_ad);
    RUN_TEST(open_scroll_puts_now_third);
    RUN_TEST(scroll_to_keeps_row_in_view);
    RUN_TEST(hit_maps_rows_and_gaps);
    RUN_TEST(content_h_counts_rows);
}
