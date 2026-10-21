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
    const ctw_airing *day1_items = t.entries[0].today.items;

    ctw_lineup ref_today, ref_yesterday;
    memset(&ref_today, 0, sizeof ref_today);
    memset(&ref_yesterday, 0, sizeof ref_yesterday);
    ASSERT_EQ(0, ctw_lineup_build(db, ch, d2, &ref_today));
    ASSERT_EQ(0, ctw_lineup_build(db, ch, d1, &ref_yesterday));
    uint32_t ref_offset = 0;
    const ctw_airing *ref = ctw_lineup_at(&ref_today, &ref_yesterday, 2000, &ref_offset);

    const ctw_airing *got = tn_current(&t, 1, d2, 2000, &offset);
    const ctw_airing *day2_items = t.entries[0].today.items;

    ASSERT(day1_items != day2_items);
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
    RUN_TEST(tuner_remaining);
}
