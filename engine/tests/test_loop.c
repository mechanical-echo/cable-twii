#include <stdio.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"
#include "helpers.h"

#define LP_DAY_MS ((int64_t)CTW_DAY_MS)

#define LP_CH(seed, ads, loop) \
    "version = 1\nseed = " seed "\n\n[[channel]]\nnumber = 2\nname = \"MTV\"\n" \
    "category = \"Music\"\nfolder = \"mtv-00s\"\nads = " ads "\nmode = \"looping\"\n" \
    "loop = [ " loop " ]\n"

#define LP_MV(count) "{ type = \"random\", show = \"Music Videos\", count = " count " }"

#define LP_PINNED \
    LP_CH("1", "true", \
        LP_MV("3") ", { type = \"ads\", count = 1 }, " \
        "{ type = \"random-run\", show = \"Music Videos\", count = 2 }")

static const ctw_date lp_day = {2026, 10, 20};

#define build_loop(cfg, day, l) build_lineup(cfg, 2, day, l)

TEST loop_covers_day(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_CH("1", "false", LP_MV("10")), lp_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, LP_DAY_MS));
    for (size_t i = 0; i < l.count; i++) {
        ASSERT_EQ(-1, l.items[i].slot_index);
        ASSERT_EQ(0, l.items[i].item_index);
        ASSERT_EQ(CTW_AIR_EPISODE, l.items[i].kind);
    }
    ASSERT_EQ(0u, l.warnings.count);
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_cut_at_midnight(void)
{
    ctw_lineup l = {0};
    const ctw_airing *last;
    ASSERT_EQ(0, build_loop(LP_CH("1", "false",
        "{ type = \"episode\", show = \"Ben 10\", from = \"Cartoon Network\", season = 1, episode = 5 }"),
        lp_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, LP_DAY_MS));
    last = &l.items[l.count - 1];
    ASSERT_EQ(66u, l.count);
    ASSERT_EQ(600000u, last->duration_ms);
    ASSERT_EQ(LP_DAY_MS, last->start_ms + (int64_t)last->duration_ms);
    ASSERT_EQ(1320000u, l.items[l.count - 2].duration_ms);
    ASSERT_EQ(0u, count_warnings(&l, CTW_W_CUT));
    ASSERT_EQ(0u, l.warnings.count);
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_cut_ends_exactly_with_ads(void)
{
    ctw_lineup l = {0};
    const ctw_airing *last;
    ASSERT_EQ(0, build_loop(LP_CH("3", "true", LP_MV("1") ", { type = \"ads\", count = 1 }"),
                            lp_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, LP_DAY_MS));
    last = &l.items[l.count - 1];
    ASSERT(last->duration_ms <= 200000u);
    ASSERT_EQ(LP_DAY_MS, last->start_ms + (int64_t)last->duration_ms);
    ASSERT_EQ(0u, l.warnings.count);
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_bag_across_passes(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_CH("1", "false", LP_MV("2")), lp_day, &l));
    ASSERT(l.count >= 6u);
    for (size_t i = 0; i < 6; i++) {
        for (size_t j = 0; j < i; j++) ASSERT(l.items[i].episode != l.items[j].episode);
    }
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_all_skipped(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_CH("1", "false", "{ type = \"ads\", count = 2 }"), lp_day, &l));
    ASSERT_EQ(1u, l.count);
    ASSERT_EQ(CTW_AIR_UPNEXT, l.items[0].kind);
    ASSERT_EQ(0, l.items[0].start_ms);
    ASSERT_EQ(CTW_DAY_MS, l.items[0].duration_ms);
    ASSERT_EQ(-1, l.items[0].slot_index);
    ASSERT_EQ(-1, l.items[0].item_index);
    ASSERT_EQ(1u, l.warnings.count);
    ASSERT(has_warning(&l, 2, CTW_W_LOOP_EMPTY, -1, -1));
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_ads_on(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_CH("1", "true", LP_MV("1") ", { type = \"ads\", count = 1 }"),
                            lp_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, LP_DAY_MS));
    for (size_t i = 0; i < l.count; i++) {
        ASSERT_EQ(i % 2 == 0 ? CTW_AIR_EPISODE : CTW_AIR_AD, l.items[i].kind);
        ASSERT_EQ(i % 2 == 0 ? 0 : 1, l.items[i].item_index);
        ASSERT_EQ(-1, l.items[i].slot_index);
    }
    for (size_t i = 3; i < l.count; i += 2) ASSERT(l.items[i].ad != l.items[i - 2].ad);
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_ad_item_always_airs(void)
{
    ctw_db *db = load_config(LP_CH("1", "false", "{ type = \"ad\", path = \"ads/ad-b.mpg\" }"));
    ctw_lineup l = {0};
    ASSERT(db);
    ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, 2), lp_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, LP_DAY_MS));
    for (size_t i = 0; i < l.count; i++) {
        ASSERT_EQ(CTW_AIR_AD, l.items[i].kind);
        ASSERT_STR_EQ("ads/ad-b.mpg", l.items[i].ad->path);
        ASSERT_EQ(-1, l.items[i].slot_index);
        ASSERT_EQ(0, l.items[i].item_index);
    }
    ctw_lineup_free(&l);
    ctw_db_free(db);
    PASS();
}

TEST loop_random_run_partial(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_CH("1", "false",
        "{ type = \"random-run\", show = \"Music Videos\", count = 9 }"), lp_day, &l));
    CHECK_CALL(assert_contiguous(&l, 0, LP_DAY_MS));
    ASSERT(has_warning(&l, 2, CTW_W_PARTIAL, -1, 0));
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_random_run_in_order(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_CH("1", "false",
        "{ type = \"random-run\", show = \"Music Videos\", count = 3 }"), lp_day, &l));
    ASSERT(l.count >= 3u);
    for (size_t i = 0; i + 2 < l.count; i += 3) {
        ASSERT_EQ(l.items[i].episode->episode + 1, l.items[i + 1].episode->episode);
        ASSERT_EQ(l.items[i].episode->episode + 2, l.items[i + 2].episode->episode);
    }
    ctw_lineup_free(&l);
    PASS();
}

TEST loop_deterministic(void)
{
    ctw_lineup a = {0};
    ctw_lineup b = {0};
    ASSERT_EQ(0, build_loop(LP_PINNED, lp_day, &a));
    ASSERT_EQ(0, build_loop(LP_PINNED, lp_day, &b));
    ASSERT_EQ(ctw_lineup_hash(&a), ctw_lineup_hash(&b));
    ctw_lineup_free(&a);
    ctw_lineup_free(&b);
    PASS();
}

TEST loop_known_lineup_pinned(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_loop(LP_PINNED, lp_day, &l));
    static const struct {
        int64_t start;
        uint32_t duration;
        ctw_air_kind kind;
        int item;
        const char *path;
    } expected[6] = {
        {0, 200000, CTW_AIR_EPISODE, 0, "mtv-00s/Music Videos/Artist 3 - Song 3.mpg"},
        {200000, 200000, CTW_AIR_EPISODE, 0, "mtv-00s/Music Videos/Artist 2 - Song 2.mpg"},
        {400000, 200000, CTW_AIR_EPISODE, 0, "mtv-00s/Music Videos/Artist 4 - Song 4.mpg"},
        {600000, 15000, CTW_AIR_AD, 1, "ads/ad-b.mpg"},
        {615000, 200000, CTW_AIR_EPISODE, 2, "mtv-00s/Music Videos/Artist 2 - Song 2.mpg"},
        {815000, 200000, CTW_AIR_EPISODE, 2, "mtv-00s/Music Videos/Artist 3 - Song 3.mpg"}
    };
    ASSERT_EQ_FMT(0xe01c5a8802c50fe5ULL, ctw_lineup_hash(&l), "%016llx");
    ASSERT_EQ(501u, l.count);
    for (size_t i = 0; i < 6; i++) {
        const ctw_airing *a = &l.items[i];
        ASSERT_EQ_FMT(expected[i].start, a->start_ms, "%lld");
        ASSERT_EQ(expected[i].duration, a->duration_ms);
        ASSERT_EQ(expected[i].kind, a->kind);
        ASSERT_EQ(-1, a->slot_index);
        ASSERT_EQ(expected[i].item, a->item_index);
        ASSERT_STR_EQ(expected[i].path,
                      a->kind == CTW_AIR_AD ? a->ad->path : a->episode->path);
    }
    ctw_lineup_free(&l);
    PASS();
}

#define LP_CAP_LIB \
    "version = 1\n" \
    "[[ad]]\npath = \"ads/a.mpg\"\nchannel = \"\"\nduration_ms = 1000\n" \
    "[[ad]]\npath = \"ads/b.mpg\"\nchannel = \"\"\nduration_ms = 1000\n" \
    "[[show]]\nchannel = \"Cap\"\nfolder = \"Show\"\nname = \"Show\"\nicon = \"\"\n" \
    "[[show.episode]]\nseason = 1\nepisode = 1\npath = \"Cap/Show/e1.mpg\"\nduration_ms = 600000\n"

TEST loop_lineup_capped(void)
{
    ctw_lineup l = {0};
    ctw_db *db = load_inline(LP_CAP_LIB,
        "version = 1\nseed = 1\n[[channel]]\nnumber = 5\nname = \"Cap\"\ncategory = \"T\"\n"
        "ads = true\nmode = \"looping\"\nloop = [ "
        "{ type = \"episode\", show = \"Show\", season = 1, episode = 1 }, "
        "{ type = \"ads\", count = 500 } ]\n");
    ASSERT(db);
    ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, 5), lp_day, &l));
    ASSERT_EQ(CTW_AIR_EPISODE, l.items[0].kind);
    CHECK_CALL(assert_capped(&l, 5, 0, LP_DAY_MS));
    ASSERT_EQ(1u, l.warnings.count);
    ctw_lineup_free(&l);
    ctw_db_free(db);
    PASS();
}

SUITE(loop_suite)
{
    RUN_TEST(loop_covers_day);
    RUN_TEST(loop_cut_at_midnight);
    RUN_TEST(loop_cut_ends_exactly_with_ads);
    RUN_TEST(loop_bag_across_passes);
    RUN_TEST(loop_all_skipped);
    RUN_TEST(loop_ads_on);
    RUN_TEST(loop_ad_item_always_airs);
    RUN_TEST(loop_random_run_partial);
    RUN_TEST(loop_random_run_in_order);
    RUN_TEST(loop_deterministic);
    RUN_TEST(loop_known_lineup_pinned);
    RUN_TEST(loop_lineup_capped);
}
