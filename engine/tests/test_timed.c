#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"
#include "pool.h"
#include "helpers.h"

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

#define TL_DAY_MS ((int64_t)CTW_DAY_MS)

#define TL_CH(seed, ads) \
    "version = 1\nseed = " seed "\n\n[[channel]]\nnumber = 1\nname = \"Cartoon Network\"\n" \
    "category = \"Kids\"\nads = " ads "\nmode = \"timed\"\n"

#define TL_SLOT(at, items) "[[channel.slot]]\nat = \"" at "\"\nitems = [ " items " ]\n"

#define TL_PINNED_SLOTS \
    TL_SLOT("07:00", \
        "{ type = \"random-run\", show = \"Gumball\", count = 3 }, " \
        "{ type = \"ads\", count = 1 }, " \
        "{ type = \"random\", show = \"Adventure Time\", season = 5, count = 5 }, " \
        "{ type = \"ads\", count = 2 }, " \
        "{ type = \"episode\", show = \"Ben 10\", season = 1, episode = 5 }, " \
        "{ type = \"random\", show = \"Gumball\", count = 2 }") \
    TL_SLOT("19:00", "{ type = \"fill\", show = \"Adventure Time\", ads_between = 2 }")

#define TL_PINNED(seed) TL_CH(seed, "true") TL_PINNED_SLOTS

static const ctw_date tl_day = {2026, 10, 20};

#define build_day(cfg, day, l) build_lineup(cfg, 1, day, l)

static ctw_db *build_keep(const char *cfg, ctw_date day, ctw_lineup *l)
{
    ctw_db *db = load_config(cfg);
    if (!db) return NULL;
    if (ctw_lineup_build(db, ctw_db_channel(db, 1), day, l)) {
        ctw_db_free(db);
        return NULL;
    }
    return db;
}

static size_t count_kind(const ctw_lineup *l, int slot, ctw_air_kind kind)
{
    size_t n = 0;
    for (size_t i = 0; i < l->count; i++) {
        if (l->items[i].slot_index == slot && l->items[i].kind == kind) n++;
    }
    return n;
}

static int has_end_at(const ctw_lineup *l, int64_t t)
{
    for (size_t i = 0; i < l->count; i++) {
        if (l->items[i].start_ms + (int64_t)l->items[i].duration_ms == t) return 1;
    }
    return 0;
}

static int64_t pool_index(const ctw_show *show, uint16_t season, const ctw_episode *e)
{
    const ctw_episode *list[16];
    size_t n = ctw_pool_list(show, season, list, 16);
    for (size_t i = 0; i < n && i < 16; i++) {
        if (list[i] == e) return (int64_t)i;
    }
    return -1;
}

TEST timed_contiguous_coverage(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_PINNED("1"), tl_day, &l));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    for (size_t i = 0; i < l.count; i++) {
        const ctw_airing *a = &l.items[i];
        ASSERT(a->slot_index == 0 || a->slot_index == 1 || a->slot_index == -1);
        if (a->slot_index == -1) {
            ASSERT_EQ(-1, a->item_index);
            ASSERT(a->kind != CTW_AIR_EPISODE);
        } else {
            ASSERT(a->item_index >= 0);
        }
        if (a->slot_index == 0) ASSERT(a->start_ms + (int64_t)a->duration_ms <= 68400000);
        if (a->slot_index == 1) ASSERT(a->start_ms >= 68400000);
    }
    ASSERT(has_end_at(&l, 68400000));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_fixed_episode_cut(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"episode\", show = \"Ben 10\", season = 1, episode = 5 }")
        TL_SLOT("07:15", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT_EQ(CTW_AIR_EPISODE, l.items[0].kind);
    ASSERT_EQ(900000u, l.items[0].duration_ms);
    ASSERT_EQ(0, l.items[0].slot_index);
    ASSERT_EQ(0, l.items[0].item_index);
    ASSERT_STR_EQ("Cartoon Network/Ben 10/Season 1/S01E05 - Ben Ten.mpg", l.items[0].episode->path);
    ASSERT_EQ(1, l.items[1].slot_index);
    ASSERT(has_warning(&l, 1, CTW_W_CUT, 0, 0));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_random_fits_only(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"random\", show = \"Adventure Time\", season = 5, count = 5 }")
        TL_SLOT("07:30", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT_EQ(2u, count_kind(&l, 0, CTW_AIR_EPISODE));
    ASSERT(has_warning(&l, 1, CTW_W_PARTIAL, 0, 0));
    for (size_t i = 0; i < l.count; i++) {
        int64_t s = l.items[i].start_ms;
        int64_t e = s + (int64_t)l.items[i].duration_ms;
        ASSERT(!(s < 27000000 && e > 27000000));
    }
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_random_no_repeat(void)
{
    ctw_lineup l = {0};
    const ctw_episode *seen[6];
    size_t n = 0;
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"random\", show = \"Gumball\", count = 6 }")
        TL_SLOT("12:00", "{ type = \"ads\" }"), tl_day, &l));
    for (size_t i = 0; i < l.count; i++) {
        if (l.items[i].slot_index != 0 || l.items[i].kind != CTW_AIR_EPISODE) continue;
        ASSERT(n < 6);
        for (size_t j = 0; j < n; j++) ASSERT(seen[j] != l.items[i].episode);
        seen[n++] = l.items[i].episode;
    }
    ASSERT_EQ(6u, n);
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_random_run_consecutive(void)
{
    ctw_date day = tl_day;
    int moved = 0;
    for (int d = 0; d < 8; d++) {
        ctw_lineup l = {0};
        ctw_db *db = build_keep(TL_CH("7", "true")
            TL_SLOT("07:00", "{ type = \"random-run\", show = \"Gumball\", count = 3 }")
            TL_SLOT("12:00", "{ type = \"ads\" }"), day, &l);
        const ctw_show *g;
        int64_t k;
        ASSERT(db);
        g = ctw_db_show(db, "Cartoon Network", "Gumball");
        ASSERT_EQ(3u, count_kind(&l, 0, CTW_AIR_EPISODE));
        k = pool_index(g, 0, l.items[0].episode);
        ASSERT(k >= 0 && k <= 3);
        if (k != 0) moved = 1;
        for (int i = 0; i < 3; i++) {
            ASSERT_EQ(CTW_AIR_EPISODE, l.items[i].kind);
            ASSERT_EQ(g, l.items[i].show);
            ASSERT_EQ(0, l.items[i].item_index);
            ASSERT_EQ_FMT(k + i, pool_index(g, 0, l.items[i].episode), "%lld");
        }
        ASSERT_EQ(0u, l.warnings.count);
        ctw_lineup_free(&l);
        ctw_db_free(db);
        day = ctw_date_next(day);
    }
    ASSERT(moved);
    PASS();
}

TEST timed_random_run_shortened(void)
{
    ctw_lineup l = {0};
    ctw_db *db = build_keep(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"random-run\", show = \"Adventure Time\", count = 5 }")
        TL_SLOT("07:30", "{ type = \"ads\" }"), tl_day, &l);
    const ctw_show *at;
    int64_t k;
    ASSERT(db);
    at = ctw_db_show(db, "Cartoon Network", "Adventure Time");
    ASSERT_EQ(2u, count_kind(&l, 0, CTW_AIR_EPISODE));
    k = pool_index(at, 0, l.items[0].episode);
    ASSERT(k >= 0);
    ASSERT_EQ_FMT(k + 1, pool_index(at, 0, l.items[1].episode), "%lld");
    ASSERT(has_warning(&l, 1, CTW_W_PARTIAL, 0, 0));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    ctw_db_free(db);
    PASS();
}

TEST timed_fill_with_ads_between(void)
{
    ctw_lineup l = {0};
    size_t i = 0;
    size_t eps = 0;
    int64_t fill_end;
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"fill\", show = \"Gumball\", ads_between = 1 }")
        TL_SLOT("08:00", "{ type = \"ads\" }"), tl_day, &l));
    while (i < l.count && l.items[i].slot_index == 0 && l.items[i].item_index == 0) {
        ASSERT_EQ((eps % 2) ? CTW_AIR_AD : CTW_AIR_EPISODE, l.items[i].kind);
        eps++;
        i++;
    }
    ASSERT(eps >= 10);
    fill_end = l.items[i - 1].start_ms + (int64_t)l.items[i - 1].duration_ms;
    ASSERT(28800000 - fill_end < 600000);
    for (; i < l.count && l.items[i].start_ms < 28800000; i++) {
        ASSERT_EQ(-1, l.items[i].slot_index);
        ASSERT_EQ(-1, l.items[i].item_index);
        ASSERT(l.items[i].kind != CTW_AIR_EPISODE);
    }
    ASSERT(has_end_at(&l, 28800000));
    ASSERT_EQ(0u, l.warnings.count);
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_ads_off(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "false")
        TL_SLOT("07:00", "{ type = \"random-run\", show = \"Gumball\", count = 1 }, "
                         "{ type = \"ads\", count = 3 }")
        TL_SLOT("08:00", "{ type = \"ads\", count = 2 }"), tl_day, &l));
    ASSERT_EQ(3u, l.count);
    ASSERT_EQ(CTW_AIR_EPISODE, l.items[0].kind);
    ASSERT_EQ(CTW_AIR_UPNEXT, l.items[1].kind);
    ASSERT_EQ(-1, l.items[1].slot_index);
    ASSERT_EQ(-1, l.items[1].item_index);
    ASSERT_EQ(3000000u, l.items[1].duration_ms);
    ASSERT_EQ(CTW_AIR_UPNEXT, l.items[2].kind);
    ASSERT_EQ(-1, l.items[2].slot_index);
    ASSERT_EQ(-1, l.items[2].item_index);
    ASSERT_EQ(CTW_DAY_MS - 3600000u, l.items[2].duration_ms);
    ASSERT_EQ(NULL, l.items[2].episode);
    ASSERT_EQ(NULL, l.items[2].ad);
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_specific_ad_on_ads_off(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "false")
        TL_SLOT("07:00", "{ type = \"ad\", path = \"ads/ad-a.mpg\" }")
        TL_SLOT("08:00", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT_EQ(1u, count_kind(&l, 0, CTW_AIR_AD));
    ASSERT_EQ(0u, count_kind(&l, 1, CTW_AIR_AD));
    ASSERT_EQ(0u, count_kind(&l, -1, CTW_AIR_AD));
    ASSERT_EQ(CTW_AIR_AD, l.items[0].kind);
    ASSERT_STR_EQ("ads/ad-a.mpg", l.items[0].ad->path);
    ASSERT_EQ(30000u, l.items[0].duration_ms);
    ASSERT_EQ(0, l.items[0].item_index);
    ASSERT_EQ(CTW_AIR_UPNEXT, l.items[1].kind);
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_nothing_fits(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"random\", show = \"Ben 10\" }")
        TL_SLOT("07:10", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT_EQ(0u, count_kind(&l, 0, CTW_AIR_EPISODE));
    ASSERT(has_warning(&l, 1, CTW_W_PARTIAL, 0, 0));
    ASSERT(has_end_at(&l, 25800000));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_single_slot(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("00:00", "{ type = \"fill\", show = \"Gumball\", ads_between = 2 }"), tl_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_last_slot_spills(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"ads\" }")
        TL_SLOT("23:00", "{ type = \"fill\", show = \"Gumball\" }"), tl_day, &l));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ASSERT(l.items[l.count - 1].start_ms >= TL_DAY_MS);
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_fixed_after_end_partial(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"episode\", show = \"Ben 10\", season = 1, episode = 5 }, "
                         "{ type = \"episode\", show = \"Gumball\", season = 1, episode = 1 }")
        TL_SLOT("07:15", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT(has_warning(&l, 1, CTW_W_CUT, 0, 0));
    ASSERT(has_warning(&l, 1, CTW_W_PARTIAL, 0, 1));
    for (size_t i = 0; i < l.count; i++) {
        ASSERT(!(l.items[i].slot_index == 0 && l.items[i].item_index == 1));
    }
    ASSERT_EQ(1, l.items[1].slot_index);
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_fixed_ad_cut(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"episode\", show = \"Gumball\", season = 1, episode = 1 }, "
                         "{ type = \"ad\", path = \"ads/ad-a.mpg\" }, "
                         "{ type = \"ad\", path = \"ads/ad-c.mpg\" }")
        TL_SLOT("07:11", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT_EQ(CTW_AIR_AD, l.items[2].kind);
    ASSERT_STR_EQ("ads/ad-c.mpg", l.items[2].ad->path);
    ASSERT_EQ(30000u, l.items[2].duration_ms);
    ASSERT_EQ(2, l.items[2].item_index);
    ASSERT(has_warning(&l, 1, CTW_W_CUT, 0, 2));
    ASSERT_EQ(1u, count_warnings(&l, CTW_W_CUT));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_fixed_ad_counts_as_last(void)
{
    ctw_date day = tl_day;
    for (int d = 0; d < 16; d++) {
        ctw_lineup l = {0};
        ASSERT_EQ(0, build_day(TL_CH("7", "true")
            TL_SLOT("07:00", "{ type = \"ad\", path = \"ads/ad-a.mpg\" }, "
                             "{ type = \"ads\", count = 1 }")
            TL_SLOT("07:01", "{ type = \"ads\" }"), day, &l));
        ASSERT_STR_EQ("ads/ad-a.mpg", l.items[0].ad->path);
        ASSERT_EQ(CTW_AIR_AD, l.items[1].kind);
        ASSERT_EQ(1, l.items[1].item_index);
        ASSERT_STR_EQ("ads/ad-b.mpg", l.items[1].ad->path);
        ctw_lineup_free(&l);
        day = ctw_date_next(day);
    }
    PASS();
}

TEST timed_random_run_pool_smaller(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_day(TL_CH("7", "true")
        TL_SLOT("07:00", "{ type = \"random-run\", show = \"Ben 10\", count = 2 }")
        TL_SLOT("12:00", "{ type = \"ads\" }"), tl_day, &l));
    ASSERT_EQ(1u, count_kind(&l, 0, CTW_AIR_EPISODE));
    ASSERT_EQ(1u, count_warnings(&l, CTW_W_PARTIAL));
    ASSERT(has_warning(&l, 1, CTW_W_PARTIAL, 0, 0));
    ctw_lineup_free(&l);
    PASS();
}

TEST timed_deterministic(void)
{
    ctw_lineup a = {0};
    ctw_lineup b = {0};
    ASSERT_EQ(0, build_day(TL_PINNED("1"), tl_day, &a));
    ASSERT_EQ(0, build_day(TL_PINNED("1"), tl_day, &b));
    ASSERT_EQ(a.count, b.count);
    ASSERT_EQ_FMT(ctw_lineup_hash(&a), ctw_lineup_hash(&b), "%llx");
    ctw_lineup_free(&a);
    ctw_lineup_free(&b);
    PASS();
}

TEST timed_seed_changes(void)
{
    ctw_lineup a = {0};
    ctw_lineup b = {0};
    ASSERT_EQ(0, build_day(TL_PINNED("1"), tl_day, &a));
    ASSERT_EQ(0, build_day(TL_PINNED("2"), tl_day, &b));
    ASSERT(ctw_lineup_hash(&a) != ctw_lineup_hash(&b));
    ctw_lineup_free(&a);
    ctw_lineup_free(&b);
    PASS();
}

TEST timed_day_changes(void)
{
    ctw_lineup a = {0};
    ctw_lineup b = {0};
    ctw_date next = {2026, 10, 21};
    ASSERT_EQ(0, build_day(TL_PINNED("1"), tl_day, &a));
    ASSERT_EQ(0, build_day(TL_PINNED("1"), next, &b));
    ASSERT(ctw_lineup_hash(&a) != ctw_lineup_hash(&b));
    ctw_lineup_free(&a);
    ctw_lineup_free(&b);
    PASS();
}

TEST timed_known_lineup_pinned(void)
{
    ctw_lineup l = {0};
    static const struct { ctw_air_kind kind; const char *path; int64_t start; uint32_t dur; } exp[5] = {
        {CTW_AIR_EPISODE, "Cartoon Network/Gumball/Season 1/S01E01 - Gumball 1.mpg", 25200000, 600000},
        {CTW_AIR_EPISODE, "Cartoon Network/Gumball/Season 1/S01E02 - Gumball 2.mpg", 25800000, 600000},
        {CTW_AIR_EPISODE, "Cartoon Network/Gumball/Season 1/S01E03 - Gumball 3.mpg", 26400000, 600000},
        {CTW_AIR_AD, "ads/ad-b.mpg", 27000000, 15000},
        {CTW_AIR_EPISODE, "Cartoon Network/Adventure Time/Season 5/S05E04 - Time 4.mpg", 27015000, 660000},
    };
    ASSERT_EQ(0, build_day(TL_PINNED("1"), tl_day, &l));
    ASSERT_EQ(1215u, l.count);
    ASSERT_EQ(0u, l.warnings.count);
    for (int i = 0; i < 5; i++) {
        const ctw_airing *a = &l.items[i];
        ASSERT_EQ(exp[i].kind, a->kind);
        ASSERT_STR_EQ(exp[i].path, a->kind == CTW_AIR_AD ? a->ad->path : a->episode->path);
        ASSERT_EQ_FMT(exp[i].start, a->start_ms, "%lld");
        ASSERT_EQ(exp[i].dur, a->duration_ms);
    }
    ASSERT_EQ_FMT(0x66360a2105c3f9adULL, ctw_lineup_hash(&l), "%016llx");
    ctw_lineup_free(&l);
    PASS();
}

#define CAP_LIB \
    "version = 1\n" \
    "[[ad]]\npath = \"ads/a.mpg\"\nchannel = \"\"\nduration_ms = 1000\n" \
    "[[ad]]\npath = \"ads/b.mpg\"\nchannel = \"\"\nduration_ms = 1000\n" \
    "[[show]]\nchannel = \"Cap\"\nfolder = \"Show\"\nname = \"Show\"\nicon = \"\"\n" \
    "[[show.episode]]\nseason = 1\nepisode = 1\npath = \"Cap/Show/e1.mpg\"\nduration_ms = 600000\n"

TEST timed_lineup_capped(void)
{
    ctw_lineup l = {0};
    ctw_db *db = load_inline(CAP_LIB,
        "version = 1\nseed = 1\n[[channel]]\nnumber = 4\nname = \"Cap\"\ncategory = \"T\"\n"
        "ads = true\nmode = \"timed\"\n"
        TL_SLOT("07:00", "{ type = \"episode\", show = \"Show\", season = 1, episode = 1 }"));
    ASSERT(db);
    ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, 4), tl_day, &l));
    ASSERT_EQ(CTW_AIR_EPISODE, l.items[0].kind);
    ASSERT_EQ(600000u, l.items[0].duration_ms);
    ASSERT_EQ(CTW_AIR_AD, l.items[1].kind);
    CHECK_CALL(assert_capped(&l, 4, 25200000, 25200000 + TL_DAY_MS));
    ASSERT_EQ(1u, l.warnings.count);
    ctw_lineup_free(&l);
    ctw_db_free(db);
    PASS();
}

TEST timed_lineup_capped_later_slots_skipped(void)
{
    ctw_lineup l = {0};
    ctw_db *db = load_inline(CAP_LIB,
        "version = 1\nseed = 1\n[[channel]]\nnumber = 4\nname = \"Cap\"\ncategory = \"T\"\n"
        "ads = true\nmode = \"timed\"\n"
        TL_SLOT("00:00", "{ type = \"ads\", count = 30000 }")
        TL_SLOT("20:00", "{ type = \"episode\", show = \"Show\", season = 1, episode = 1 }"));
    ASSERT(db);
    ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, 4), tl_day, &l));
    CHECK_CALL(assert_capped(&l, 4, 0, TL_DAY_MS));
    ASSERT_EQ(1u, l.warnings.count);
    for (size_t i = 0; i < l.count; i++) ASSERT(l.items[i].kind != CTW_AIR_EPISODE);
    ctw_lineup_free(&l);
    ctw_db_free(db);
    PASS();
}

#define ALT_EP(n, dur) \
    "[[show.episode]]\nseason = 1\nepisode = " #n "\npath = \"Alt/Show/e" #n ".mpg\"\n" \
    "duration_ms = " dur "\n"

#define ALT_LIB \
    "version = 1\n" \
    "[[show]]\nchannel = \"Alt\"\nfolder = \"Show\"\nname = \"Show\"\nicon = \"\"\n" \
    ALT_EP(1, "2640000") ALT_EP(2, "1320000") ALT_EP(3, "2640000") \
    ALT_EP(4, "1320000") ALT_EP(5, "2640000") ALT_EP(6, "1320000")

TEST timed_random_run_skips_unfitting_starts(void)
{
    ctw_db *db = load_inline(ALT_LIB,
        "version = 1\nseed = 7\n[[channel]]\nnumber = 6\nname = \"Alt\"\ncategory = \"T\"\n"
        "ads = false\nmode = \"timed\"\n"
        TL_SLOT("07:00", "{ type = \"random-run\", show = \"Show\", count = 2 }")
        TL_SLOT("07:25", "{ type = \"ads\" }"));
    ctw_date day = tl_day;
    int starts[7] = {0};
    ASSERT(db);
    for (int d = 0; d < 16; d++) {
        ctw_lineup l = {0};
        ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, 6), day, &l));
        ASSERT(count_kind(&l, 0, CTW_AIR_EPISODE) >= 1u);
        ASSERT_EQ(CTW_AIR_EPISODE, l.items[0].kind);
        ASSERT_EQ(0, l.items[0].slot_index);
        ASSERT_EQ(1320000u, l.items[0].duration_ms);
        starts[l.items[0].episode->episode]++;
        ASSERT(has_warning(&l, 6, CTW_W_PARTIAL, 0, 0));
        CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
        ctw_lineup_free(&l);
        day = ctw_date_next(day);
    }
    ASSERT_EQ(16, starts[2] + starts[4]);
    ASSERT(starts[2] > 0 && starts[4] > 0);
    ctw_db_free(db);
    PASS();
}

TEST timed_random_run_no_fitting_start(void)
{
    ctw_lineup l = {0};
    ctw_db *db = load_inline(ALT_LIB,
        "version = 1\nseed = 7\n[[channel]]\nnumber = 6\nname = \"Alt\"\ncategory = \"T\"\n"
        "ads = false\nmode = \"timed\"\n"
        TL_SLOT("07:00", "{ type = \"random-run\", show = \"Show\", count = 2 }")
        TL_SLOT("07:20", "{ type = \"ads\" }"));
    ASSERT(db);
    ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, 6), tl_day, &l));
    ASSERT_EQ(0u, count_kind(&l, 0, CTW_AIR_EPISODE));
    ASSERT_EQ(1u, count_warnings(&l, CTW_W_PARTIAL));
    ASSERT(has_warning(&l, 6, CTW_W_PARTIAL, 0, 0));
    CHECK_CALL(assert_contiguous(&l, 25200000, 25200000 + TL_DAY_MS));
    ctw_lineup_free(&l);
    ctw_db_free(db);
    PASS();
}

TEST lineup_hash_encoding(void)
{
    ctw_episode ep = {0};
    ctw_ad ad = {0};
    ctw_airing items[3];
    ctw_lineup l = {0};
    unsigned char buf[64];
    size_t n = 0;
    uint64_t h;
    ep.path = "a/b.mpg";
    ad.path = "ads/x.mpg";
    memset(items, 0, sizeof items);
    items[0].start_ms = 0x0102030405LL;
    items[0].duration_ms = 0x0A0B0C0Du;
    items[0].kind = CTW_AIR_EPISODE;
    items[0].episode = &ep;
    items[1].start_ms = 7;
    items[1].duration_ms = 1;
    items[1].kind = CTW_AIR_AD;
    items[1].ad = &ad;
    items[2].start_ms = 8;
    items[2].duration_ms = 2;
    items[2].kind = CTW_AIR_UPNEXT;
    l.items = items;
    l.count = 3;
    {
        unsigned char a0[] = {0, 0, 0, 1, 2, 3, 4, 5, 0x0A, 0x0B, 0x0C, 0x0D, 0,
                              'a', '/', 'b', '.', 'm', 'p', 'g', 0};
        unsigned char a1[] = {0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 1, 1,
                              'a', 'd', 's', '/', 'x', '.', 'm', 'p', 'g', 0};
        unsigned char a2[] = {0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 2, 2};
        memcpy(buf + n, a0, sizeof a0);
        n += sizeof a0;
        memcpy(buf + n, a1, sizeof a1);
        n += sizeof a1;
        memcpy(buf + n, a2, sizeof a2);
        n += sizeof a2;
    }
    h = ctw_fnv1a(CTW_FNV_OFFSET, buf, n);
    ASSERT_EQ_FMT(h, ctw_lineup_hash(&l), "%016llx");
    l.count = 0;
    ASSERT_EQ_FMT(CTW_FNV_OFFSET, ctw_lineup_hash(&l), "%016llx");
    PASS();
}

TEST lineup_free_null_safe(void)
{
    ctw_lineup l = {0};
    ctw_lineup_free(NULL);
    ctw_lineup_free(&l);
    PASS();
}

SUITE(timed_suite)
{
    RUN_TEST(timed_contiguous_coverage);
    RUN_TEST(timed_fixed_episode_cut);
    RUN_TEST(timed_random_fits_only);
    RUN_TEST(timed_random_no_repeat);
    RUN_TEST(timed_random_run_consecutive);
    RUN_TEST(timed_random_run_shortened);
    RUN_TEST(timed_fill_with_ads_between);
    RUN_TEST(timed_ads_off);
    RUN_TEST(timed_specific_ad_on_ads_off);
    RUN_TEST(timed_nothing_fits);
    RUN_TEST(timed_single_slot);
    RUN_TEST(timed_last_slot_spills);
    RUN_TEST(timed_fixed_after_end_partial);
    RUN_TEST(timed_fixed_ad_cut);
    RUN_TEST(timed_fixed_ad_counts_as_last);
    RUN_TEST(timed_random_run_pool_smaller);
    RUN_TEST(timed_deterministic);
    RUN_TEST(timed_seed_changes);
    RUN_TEST(timed_day_changes);
    RUN_TEST(timed_known_lineup_pinned);
    RUN_TEST(timed_lineup_capped);
    RUN_TEST(timed_lineup_capped_later_slots_skipped);
    RUN_TEST(timed_random_run_skips_unfitting_starts);
    RUN_TEST(timed_random_run_no_fitting_start);
    RUN_TEST(lineup_hash_encoding);
    RUN_TEST(lineup_free_null_safe);
}
