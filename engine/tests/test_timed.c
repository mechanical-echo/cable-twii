#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"
#include "pool.h"

#define LIB_PATH "tests/fixtures/lib_basic.toml"

static char *read_file(const char *path, const char *extra)
{
    FILE *f = fopen(path, "rb");
    long n;
    size_t extra_len = extra ? strlen(extra) : 0;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + extra_len + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf) {
        if (extra_len) memcpy(buf + n, extra, extra_len);
        buf[(size_t)n + extra_len] = '\0';
    }
    fclose(f);
    return buf;
}

static ctw_db *load_library(const char *extra)
{
    ctw_issues is = {0};
    char *text = read_file(LIB_PATH, extra);
    ctw_db *db = ctw_db_new();
    int rc = text ? ctw_library_parse(db, text, &is) : -1;
    free(text);
    ctw_issues_free(&is);
    if (rc) {
        ctw_db_free(db);
        return NULL;
    }
    return db;
}

static ctw_channel cn_channel(void)
{
    ctw_channel ch = {0};
    ch.number = 1;
    ch.name = "Cartoon Network";
    ch.folder = "Cartoon Network";
    return ch;
}

TEST pool_list_filters_by_season(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    const ctw_episode *out[8];
    ASSERT_EQ(6u, ctw_pool_list(g, 0, out, 8));
    ASSERT_EQ(4u, ctw_pool_list(g, 1, out, 8));
    ASSERT_EQ(2u, ctw_pool_list(g, 2, out, 8));
    ASSERT_EQ(2, out[0]->season);
    ASSERT_EQ(1, out[0]->episode);
    ASSERT_EQ(0u, ctw_pool_list(g, 9, out, 8));
    ASSERT_EQ(6u, ctw_pool_list(g, 0, out, 3));
    ASSERT_EQ(6u, ctw_pool_list(g, 0, NULL, 0));
    ASSERT_EQ(0u, ctw_pool_list(g, 9, NULL, 0));
    ctw_db_free(db);
    PASS();
}

TEST bag_no_repeat_until_empty(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(1, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, g, 0);
    const ctw_episode *seen[6];
    ASSERT(b);
    for (int i = 0; i < 6; i++) {
        seen[i] = ctw_bag_draw_fitting(b, &r, UINT32_MAX);
        ASSERT(seen[i]);
        for (int j = 0; j < i; j++) ASSERT(seen[i] != seen[j]);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_refills_after_empty(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(1, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, g, 0);
    for (int i = 0; i < 6; i++) ASSERT(ctw_bag_draw_fitting(b, &r, UINT32_MAX));
    ASSERT(ctw_bag_draw_fitting(b, &r, UINT32_MAX));
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_respects_fit(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *ben = ctw_db_show(db, "Cartoon Network", "Ben 10");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(1, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, ben, 0);
    const ctw_episode *e;
    ASSERT_EQ(NULL, ctw_bag_draw_fitting(b, &r, 600000));
    e = ctw_bag_draw_fitting(b, &r, 1320000);
    ASSERT(e);
    ASSERT_EQ(1, e->season);
    ASSERT_EQ(5, e->episode);
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_season_filter(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(1, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, g, 2);
    ASSERT(b == ctw_bagset_get(&s, g, 2));
    for (int i = 0; i < 4; i++) {
        const ctw_episode *e = ctw_bag_draw_fitting(b, &r, UINT32_MAX);
        ASSERT(e);
        ASSERT_EQ(2, e->season);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_empty_pool_returns_null(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(1, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, g, 9);
    ASSERT(b);
    ASSERT_EQ(NULL, ctw_bag_draw_fitting(b, &r, UINT32_MAX));
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_no_fit_does_not_reshuffle(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(1, CTW_RNG_STREAM);
    ctw_rng copy;
    ctw_bag *b = ctw_bagset_get(&s, g, 0);
    ASSERT(ctw_bag_draw_fitting(b, &r, UINT32_MAX));
    copy = r;
    ASSERT_EQ(NULL, ctw_bag_draw_fitting(b, &r, 1000));
    ASSERT_EQ(copy.state, r.state);
    ASSERT_EQ(1u, b->pos);
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_deterministic(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s1 = {0};
    ctw_bagset s2 = {0};
    ctw_rng r1 = ctw_rng_seed(99, CTW_RNG_STREAM);
    ctw_rng r2 = ctw_rng_seed(99, CTW_RNG_STREAM);
    ctw_bag *b1 = ctw_bagset_get(&s1, g, 0);
    ctw_bag *b2 = ctw_bagset_get(&s2, g, 0);
    for (int i = 0; i < 6; i++) {
        ASSERT(ctw_bag_draw_fitting(b1, &r1, UINT32_MAX) == ctw_bag_draw_fitting(b2, &r2, UINT32_MAX));
    }
    ctw_bagset_free(&s1);
    ctw_bagset_free(&s2);
    ctw_db_free(db);
    PASS();
}

TEST bag_known_sequence_pinned(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(12345, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, g, 0);
    int expected[6][2] = {{2, 1}, {2, 2}, {1, 1}, {1, 2}, {1, 3}, {1, 4}};
    for (int i = 0; i < 6; i++) {
        const ctw_episode *e = ctw_bag_draw_fitting(b, &r, UINT32_MAX);
        ASSERT(e);
        ASSERT_EQ(expected[i][0], e->season);
        ASSERT_EQ(expected[i][1], e->episode);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bag_reshuffle_pinned(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(12345, CTW_RNG_STREAM);
    ctw_bag *b = ctw_bagset_get(&s, g, 0);
    int expected[6][2] = {{2, 1}, {1, 1}, {1, 2}, {1, 4}, {2, 2}, {1, 3}};
    for (int i = 0; i < 6; i++) ASSERT(ctw_bag_draw_fitting(b, &r, UINT32_MAX));
    for (int i = 0; i < 6; i++) {
        const ctw_episode *e = ctw_bag_draw_fitting(b, &r, UINT32_MAX);
        ASSERT(e);
        ASSERT_EQ(expected[i][0], e->season);
        ASSERT_EQ(expected[i][1], e->episode);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bagset_pointers_stable(void)
{
    ctw_db *db = load_library(NULL);
    const ctw_show *g = ctw_db_show(db, "Cartoon Network", "Gumball");
    const ctw_show *at = ctw_db_show(db, "Cartoon Network", "Adventure Time");
    const ctw_show *ben = ctw_db_show(db, "Cartoon Network", "Ben 10");
    const ctw_show *mtv = ctw_db_show(db, "mtv-00s", "Music Videos");
    const ctw_show *shows[4] = {g, at, ben, mtv};
    uint16_t seasons[4] = {0, 1, 2, 5};
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(3, CTW_RNG_STREAM);
    ctw_bag *first = ctw_bagset_get(&s, g, 0);
    ASSERT(first);
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) ASSERT(ctw_bagset_get(&s, shows[i], seasons[j]));
    }
    ASSERT(s.count >= 12);
    ASSERT(first == ctw_bagset_get(&s, g, 0));
    ASSERT(ctw_bag_draw_fitting(first, &r, UINT32_MAX));
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST bagset_free_null_safe(void)
{
    ctw_bagset_free(NULL);
    PASS();
}

TEST ads_prefer_channel_folder(void)
{
    ctw_db *db = load_library("\n[[ad]]\npath = \"ads/cn.mpg\"\nchannel = \"Cartoon Network\"\nduration_ms = 20000\n");
    ctw_channel ch = cn_channel();
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(5, CTW_RNG_STREAM);
    ASSERT(db);
    for (int i = 0; i < 20; i++) {
        const ctw_ad *a = ctw_ads_draw_fitting(db, &ch, &s, &r, UINT32_MAX);
        ASSERT(a);
        ASSERT_STR_EQ("ads/cn.mpg", a->path);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST ads_shared_when_no_channel_ads(void)
{
    ctw_db *db = load_library(NULL);
    ctw_channel ch = cn_channel();
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(5, CTW_RNG_STREAM);
    for (int i = 0; i < 20; i++) {
        const ctw_ad *a = ctw_ads_draw_fitting(db, &ch, &s, &r, UINT32_MAX);
        ASSERT(a);
        ASSERT_STR_EQ("", a->channel_folder);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST ads_no_immediate_repeat(void)
{
    ctw_db *db = load_library(NULL);
    ctw_channel ch = cn_channel();
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(5, CTW_RNG_STREAM);
    const ctw_ad *prev = NULL;
    for (int i = 0; i < 200; i++) {
        const ctw_ad *a = ctw_ads_draw_fitting(db, &ch, &s, &r, UINT32_MAX);
        ASSERT(a);
        ASSERT(a != prev);
        prev = a;
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST ads_fit_limit(void)
{
    ctw_db *db = load_library(NULL);
    ctw_channel ch = cn_channel();
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(5, CTW_RNG_STREAM);
    for (int i = 0; i < 10; i++) {
        const ctw_ad *a = ctw_ads_draw_fitting(db, &ch, &s, &r, 20000);
        ASSERT(a);
        ASSERT_STR_EQ("ads/ad-b.mpg", a->path);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST ads_none_fit(void)
{
    ctw_db *db = load_library(NULL);
    ctw_channel ch = cn_channel();
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(5, CTW_RNG_STREAM);
    const ctw_ad *a = ctw_ads_draw_fitting(db, &ch, &s, &r, UINT32_MAX);
    ASSERT(a);
    ASSERT_EQ(NULL, ctw_ads_draw_fitting(db, &ch, &s, &r, 10000));
    ASSERT(s.has_last_ad);
    ASSERT(a == ctw_db_ad_at(db, s.last_ad));
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

TEST ads_known_sequence_pinned(void)
{
    ctw_db *db = load_library(NULL);
    ctw_channel ch = cn_channel();
    ctw_bagset s = {0};
    ctw_rng r = ctw_rng_seed(12345, CTW_RNG_STREAM);
    const char *expected[10] = {
        "ads/ad-a.mpg", "ads/ad-b.mpg", "ads/ad-c.mpg", "ads/ad-a.mpg", "ads/ad-c.mpg",
        "ads/ad-a.mpg", "ads/ad-c.mpg", "ads/ad-b.mpg", "ads/ad-a.mpg", "ads/ad-c.mpg"
    };
    for (int i = 0; i < 10; i++) {
        const ctw_ad *a = ctw_ads_draw_fitting(db, &ch, &s, &r, UINT32_MAX);
        ASSERT(a);
        ASSERT_STR_EQ(expected[i], a->path);
    }
    ctw_bagset_free(&s);
    ctw_db_free(db);
    PASS();
}

SUITE(pool_suite)
{
    RUN_TEST(pool_list_filters_by_season);
    RUN_TEST(bag_no_repeat_until_empty);
    RUN_TEST(bag_refills_after_empty);
    RUN_TEST(bag_respects_fit);
    RUN_TEST(bag_season_filter);
    RUN_TEST(bag_empty_pool_returns_null);
    RUN_TEST(bag_no_fit_does_not_reshuffle);
    RUN_TEST(bag_deterministic);
    RUN_TEST(bag_known_sequence_pinned);
    RUN_TEST(bag_reshuffle_pinned);
    RUN_TEST(bagset_pointers_stable);
    RUN_TEST(bagset_free_null_safe);
    RUN_TEST(ads_prefer_channel_folder);
    RUN_TEST(ads_shared_when_no_channel_ads);
    RUN_TEST(ads_no_immediate_repeat);
    RUN_TEST(ads_fit_limit);
    RUN_TEST(ads_none_fit);
    RUN_TEST(ads_known_sequence_pinned);
}
