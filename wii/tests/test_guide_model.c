#include <string.h>
#include "greatest/greatest.h"
#include "guide_model.h"

#define EPS 0.001f

static ctw_db *parse_channels(const char *toml)
{
    ctw_db *db = ctw_db_new();
    ctw_issues issues;
    memset(&issues, 0, sizeof issues);
    ctw_library_parse(db, "version = 1\n", &issues);
    ctw_issues_free(&issues);
    memset(&issues, 0, sizeof issues);
    ctw_config_parse(db, toml, &issues);
    ctw_issues_free(&issues);
    return db;
}

static const char *CH_KIDS_MUSIC =
    "version = 1\nseed = 1\n"
    "[[channel]]\nnumber = 1\nname = \"A\"\ncategory = \"Kids\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n"
    "[[channel]]\nnumber = 2\nname = \"B\"\ncategory = \"Music\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n"
    "[[channel]]\nnumber = 3\nname = \"C\"\ncategory = \"Kids\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n";

static size_t build_kids_music(ctw_db **db, gm_row *rows, size_t cap)
{
    *db = parse_channels(CH_KIDS_MUSIC);
    return gm_build(*db, rows, cap);
}

TEST gm_groups_by_category(void)
{
    ctw_db *db;
    gm_row rows[8];
    size_t n = build_kids_music(&db, rows, 8);
    ASSERT_EQ(5u, n);
    ASSERT_EQ(GM_HEADER, rows[0].kind);
    ASSERT_STR_EQ("Kids", rows[0].category);
    ASSERT_EQ(GM_CHANNEL, rows[1].kind);
    ASSERT_EQ(1, rows[1].channel->number);
    ASSERT_EQ(GM_CHANNEL, rows[2].kind);
    ASSERT_EQ(3, rows[2].channel->number);
    ASSERT_EQ(GM_HEADER, rows[3].kind);
    ASSERT_STR_EQ("Music", rows[3].category);
    ASSERT_EQ(GM_CHANNEL, rows[4].kind);
    ASSERT_EQ(2, rows[4].channel->number);
    ASSERT_IN_RANGE(0.0f, rows[0].y, EPS);
    ASSERT_IN_RANGE(28.0f, rows[0].h, EPS);
    ASSERT_IN_RANGE(34.0f, rows[1].y, EPS);
    ASSERT_IN_RANGE(64.0f, rows[1].h, EPS);
    ASSERT_IN_RANGE(104.0f, rows[2].y, EPS);
    ASSERT_IN_RANGE(174.0f, rows[3].y, EPS);
    ASSERT_IN_RANGE(208.0f, rows[4].y, EPS);
    ctw_db_free(db);
    PASS();
}

TEST gm_build_respects_cap(void)
{
    ctw_db *db;
    gm_row rows[3];
    size_t n = build_kids_music(&db, rows, 3);
    ASSERT_EQ(3u, n);
    ASSERT_EQ(3, rows[2].channel->number);
    ctw_db_free(db);
    PASS();
}

TEST gm_hit_rows(void)
{
    ctw_db *db;
    gm_row rows[8];
    size_t n = build_kids_music(&db, rows, 8);
    float top = 50.0f;
    ASSERT_EQ(1, gm_hit(rows, n, 0.0f, top, 100.0f, top + 40.0f));
    ASSERT_EQ(-1, gm_hit(rows, n, 0.0f, top, 100.0f, top + 10.0f));
    ASSERT_EQ(-1, gm_hit(rows, n, 0.0f, top, 100.0f, top + 100.0f));
    ASSERT_EQ(2, gm_hit(rows, n, 0.0f, top, 100.0f, top + 110.0f));
    ASSERT_EQ(2, gm_hit(rows, n, 70.0f, top, 100.0f, top + 40.0f));
    ASSERT_EQ(-1, gm_hit(rows, n, 70.0f, top, 100.0f, top + 110.0f));
    ASSERT_EQ(4, gm_hit(rows, n, 70.0f, top, 100.0f, top + 140.0f));
    ctw_db_free(db);
    PASS();
}

TEST gm_step_skips_headers(void)
{
    ctw_db *db;
    gm_row rows[8];
    size_t n = build_kids_music(&db, rows, 8);
    ASSERT_EQ(4, gm_step(rows, n, 2, 1));
    ASSERT_EQ(1, gm_step(rows, n, 1, -1));
    ASSERT_EQ(2, gm_step(rows, n, 4, -1));
    ASSERT_EQ(4, gm_step(rows, n, 4, 1));
    ASSERT_EQ(1, gm_step(rows, n, -1, 1));
    ctw_db_free(db);
    PASS();
}

TEST gm_scroll_to_visible(void)
{
    gm_row rows[2];
    memset(rows, 0, sizeof rows);
    rows[0].kind = GM_CHANNEL;
    rows[0].y = 0.0f;
    rows[0].h = 64.0f;
    rows[1].kind = GM_CHANNEL;
    rows[1].y = 250.0f;
    rows[1].h = 64.0f;
    ASSERT_IN_RANGE(114.0f, gm_scroll_to(rows, 2, 1, 0.0f, 200.0f), EPS);
    ASSERT_IN_RANGE(130.0f, gm_scroll_to(rows, 2, 1, 130.0f, 200.0f), EPS);
    ASSERT_IN_RANGE(0.0f, gm_scroll_to(rows, 2, 0, 114.0f, 200.0f), EPS);
    PASS();
}

TEST gm_find_channel_row(void)
{
    ctw_db *db;
    gm_row rows[8];
    size_t n = build_kids_music(&db, rows, 8);
    ASSERT_EQ(4, gm_find(rows, n, 2));
    ASSERT_EQ(2, gm_find(rows, n, 3));
    ASSERT_EQ(-1, gm_find(rows, n, 9));
    ctw_db_free(db);
    PASS();
}

TEST gm_content_height(void)
{
    ctw_db *db;
    gm_row rows[8];
    size_t n = build_kids_music(&db, rows, 8);
    ASSERT_IN_RANGE(272.0f, gm_content_h(rows, n), EPS);
    ASSERT_IN_RANGE(0.0f, gm_content_h(rows, 0), EPS);
    ctw_db_free(db);
    PASS();
}

TEST gm_edge_scroll_near_edges(void)
{
    float top = 50.0f, view = 200.0f, content = 272.0f;
    ASSERT_IN_RANGE(16.0f, gm_edge_scroll(10.0f, top + 190.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(4.0f, gm_edge_scroll(10.0f, top + 5.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(10.0f, gm_edge_scroll(10.0f, top + 100.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(10.0f, gm_edge_scroll(10.0f, top - 5.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(10.0f, gm_edge_scroll(10.0f, top + view + 5.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(0.0f, gm_edge_scroll(3.0f, top + 5.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(72.0f, gm_edge_scroll(70.0f, top + 190.0f, top, view, content), EPS);
    ASSERT_IN_RANGE(0.0f, gm_edge_scroll(0.0f, top + 190.0f, top, 300.0f, content), EPS);
    PASS();
}

SUITE(guide_model_suite)
{
    RUN_TEST(gm_groups_by_category);
    RUN_TEST(gm_build_respects_cap);
    RUN_TEST(gm_hit_rows);
    RUN_TEST(gm_step_skips_headers);
    RUN_TEST(gm_scroll_to_visible);
    RUN_TEST(gm_find_channel_row);
    RUN_TEST(gm_content_height);
    RUN_TEST(gm_edge_scroll_near_edges);
}
