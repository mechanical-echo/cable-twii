#include <string.h>
#include "greatest/greatest.h"
#include "tuner.h"

#define FIXTURE "../../engine/tests/fixtures/root_basic"

static ctw_db *load_fixture(void)
{
    ctw_db *db = NULL;
    ctw_issues issues;
    memset(&issues, 0, sizeof issues);
    int rc = ctw_db_load(FIXTURE, &db, &issues);
    ctw_issues_free(&issues);
    if (rc != 0) return NULL;
    return db;
}

TEST tuner_current_matches_engine(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    const ctw_channel *ch = ctw_db_channel(db, 1);
    ASSERT(ch != NULL);

    ctw_date day = { 2026, 10, 20 };
    ctw_date prev = ctw_date_prev(day);
    ctw_lineup today, yesterday;
    memset(&today, 0, sizeof today);
    memset(&yesterday, 0, sizeof yesterday);
    ASSERT_EQ(0, ctw_lineup_build(db, ch, day, &today));
    ASSERT_EQ(0, ctw_lineup_build(db, ch, prev, &yesterday));
    uint32_t ref_offset = 0;
    const ctw_airing *ref = ctw_lineup_at(&today, &yesterday, 7u * 3600000u + 5000u, &ref_offset);
    ASSERT(ref != NULL);

    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    const ctw_airing *got = tn_current(&t, 1, day, 7u * 3600000u + 5000u, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(ref->start_ms, got->start_ms);
    ASSERT_EQ(ref_offset, offset);

    tn_free(&t);
    ctw_lineup_free(&today);
    ctw_lineup_free(&yesterday);
    ctw_db_free(db);
    PASS();
}

TEST tuner_caches_by_day(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    tn_current(&t, 1, day, 1000, &offset);
    const ctw_airing *items_first = t.entries[0].today.items;
    tn_current(&t, 1, day, 2000, &offset);
    const ctw_airing *items_second = t.entries[0].today.items;
    ASSERT_EQ((size_t)items_first, (size_t)items_second);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_rebuilds_on_day_change(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    const ctw_channel *ch = ctw_db_channel(db, 1);
    ASSERT(ch != NULL);

    tuner t;
    tn_init(&t, db);
    ctw_date d1 = { 2026, 10, 20 };
    ctw_date d2 = { 2026, 10, 21 };
    uint32_t offset = 0;
    tn_current(&t, 1, d1, 1000, &offset);

    ctw_lineup ref_today, ref_yesterday;
    memset(&ref_today, 0, sizeof ref_today);
    memset(&ref_yesterday, 0, sizeof ref_yesterday);
    ASSERT_EQ(0, ctw_lineup_build(db, ch, d2, &ref_today));
    ASSERT_EQ(0, ctw_lineup_build(db, ch, d1, &ref_yesterday));
    uint32_t ref_offset = 0;
    const ctw_airing *ref = ctw_lineup_at(&ref_today, &ref_yesterday, 2000, &ref_offset);

    const ctw_airing *got = tn_current(&t, 1, d2, 2000, &offset);

    ASSERT_EQ(ctw_date_key(d2), ctw_date_key(t.entries[0].day));
    ASSERT(got != NULL);
    ASSERT(ref != NULL);
    ASSERT_EQ(ref->start_ms, got->start_ms);
    ASSERT_EQ(ref_offset, offset);

    ctw_lineup_free(&ref_today);
    ctw_lineup_free(&ref_yesterday);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_spans_midnight(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date d1 = { 2026, 10, 19 };
    ctw_date d2 = { 2026, 10, 20 };

    uint32_t before_offset = 0;
    const ctw_airing *before = tn_current(&t, 1, d1, 23u * 3600000u + 59u * 60000u, &before_offset);
    ASSERT(before != NULL);
    ASSERT_EQ(CTW_AIR_EPISODE, before->kind);
    int64_t before_start = tn_airing_start(&t, 1, before);
    ASSERT_EQ(1792453935000LL, before_start);
    const ctw_episode *episode = before->episode;

    uint32_t after_offset = 0;
    const ctw_airing *after = tn_current(&t, 1, d2, 30000u, &after_offset);
    ASSERT(after != NULL);
    ASSERT(after >= t.entries[0].yesterday.items && after < t.entries[0].yesterday.items + t.entries[0].yesterday.count);
    ASSERT_EQ(CTW_AIR_EPISODE, after->kind);
    ASSERT_STR_EQ(episode->path, after->episode->path);
    ASSERT_EQ(405000u, before_offset);
    ASSERT_EQ(495000u, after_offset);
    ASSERT_EQ(90000u, after_offset - before_offset);
    ASSERT_EQ(before_start, tn_airing_start(&t, 1, after));

    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_airing_start_is_absolute(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date d1 = { 2026, 10, 19 };
    ctw_date d2 = { 2026, 10, 20 };
    uint32_t offset = 0;
    const ctw_airing *a1 = tn_current(&t, 1, d1, 8u * 3600000u, &offset);
    ASSERT(a1 != NULL);
    int64_t s1 = tn_airing_start(&t, 1, a1);
    const ctw_airing *a2 = tn_current(&t, 1, d2, 8u * 3600000u, &offset);
    ASSERT(a2 != NULL);
    int64_t s2 = tn_airing_start(&t, 1, a2);
    ASSERT(s2 - s1 > 0);
    ASSERT(s1 != s2);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_next_after_spill_continues_in_yesterday(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    const ctw_airing *gap = tn_current(&t, 1, day, 30000u, &offset);
    ASSERT(gap != NULL);
    const ctw_airing *next = tn_next(&t, 1, gap);
    ASSERT(next != NULL);
    ASSERT_EQ(gap->start_ms + (int64_t)gap->duration_ms, next->start_ms);
    ASSERT_EQ(86595000, next->start_ms);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_next_after_last_spill_falls_back_to_today(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    const ctw_airing *last = tn_current(&t, 1, day, 6u * 3600000u + 59u * 60000u + 59000u, &offset);
    ASSERT(last != NULL);
    const tuner_entry *e = &t.entries[0];
    ASSERT_EQ((size_t)(e->yesterday.items + e->yesterday.count - 1), (size_t)last);
    const ctw_airing *next = tn_next(&t, 1, last);
    ASSERT(next != NULL);
    ASSERT(next >= e->today.items && next < e->today.items + e->today.count);
    ASSERT_EQ(7 * 3600000, next->start_ms);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_next_within_today(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    const ctw_airing *cur = tn_current(&t, 1, day, 7u * 3600000u + 5000u, &offset);
    ASSERT(cur != NULL);
    const ctw_airing *next = tn_next(&t, 1, cur);
    ASSERT_EQ((size_t)(cur + 1), (size_t)next);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST tuner_remaining(void)
{
    ctw_airing a;
    memset(&a, 0, sizeof a);
    a.duration_ms = 600000;
    ASSERT_EQ(595000u, tn_ms_remaining(&a, 5000));
    PASS();
}

SUITE(tuner_suite)
{
    RUN_TEST(tuner_current_matches_engine);
    RUN_TEST(tuner_caches_by_day);
    RUN_TEST(tuner_rebuilds_on_day_change);
    RUN_TEST(tuner_spans_midnight);
    RUN_TEST(tuner_airing_start_is_absolute);
    RUN_TEST(tuner_next_after_spill_continues_in_yesterday);
    RUN_TEST(tuner_next_after_last_spill_falls_back_to_today);
    RUN_TEST(tuner_next_within_today);
    RUN_TEST(tuner_remaining);
}
