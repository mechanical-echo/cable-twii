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

TEST next_episode_skips_ads(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    const ctw_airing *first = tn_current(&t, 1, day, 7u * 3600000u + 5000u, &offset);
    ASSERT(first != NULL);
    const ctw_airing *second = tn_next_episode(&t, 1, first);
    ASSERT(second != NULL);
    ASSERT_EQ(CTW_AIR_EPISODE, second->kind);
    ASSERT_EQ(1, second->episode->season);
    ASSERT_EQ(2, second->episode->episode);
    const ctw_airing *third = tn_current(&t, 1, day, 7u * 3600000u + 20u * 60000u + 5000u, &offset);
    ASSERT(third != NULL);
    const ctw_airing *after_ad = tn_next(&t, 1, third);
    ASSERT(after_ad != NULL);
    ASSERT_EQ(CTW_AIR_AD, after_ad->kind);
    const ctw_airing *ep = tn_next_episode(&t, 1, third);
    ASSERT(ep != NULL);
    ASSERT_EQ(CTW_AIR_EPISODE, ep->kind);
    ASSERT_EQ(5, ep->episode->season);
    ASSERT_EQ(4, ep->episode->episode);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST next_episode_none_at_end(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    ASSERT(tn_current(&t, 1, day, 1000, &offset) != NULL);
    const ctw_lineup *today = &t.entries[0].today;
    ASSERT(today->count > 0);
    const ctw_airing *last = &today->items[today->count - 1];
    ASSERT_EQ((size_t)NULL, (size_t)tn_next_episode(&t, 1, last));
    ASSERT_EQ((size_t)NULL, (size_t)tn_next_episode(&t, 1, NULL));
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST ms_until_next_episode(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    uint32_t offset = 0;
    const ctw_airing *third = tn_current(&t, 1, day, 7u * 3600000u + 21u * 60000u, &offset);
    ASSERT(third != NULL);
    ASSERT_EQ(60000u, offset);
    const ctw_airing *ep = tn_next_episode(&t, 1, third);
    ASSERT(ep != NULL);
    ASSERT_EQ(555000LL, tn_ms_until(&t, 1, third, offset, ep));
    ASSERT_EQ(0LL, tn_ms_until(&t, 1, third, offset, NULL));
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST rebuild_keeps_the_next_episode_start_across_midnight(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 2026, 10, 20 };
    ctw_date next_day = { 2026, 10, 21 };
    uint32_t offset = 0;
    const ctw_airing *before = tn_current(&t, 1, day, 23u * 3600000u + 50u * 60000u, &offset);
    ASSERT(before != NULL);
    const ctw_airing *ep_before = tn_next_episode(&t, 1, before);
    ASSERT(ep_before != NULL);
    int64_t ep_start = tn_airing_start(&t, 1, ep_before);
    ASSERT(ep_start > 0);
    const ctw_airing *after = tn_current(&t, 1, next_day, 5u * 60000u, &offset);
    ASSERT(after != NULL);
    ASSERT_EQ(CTW_AIR_EPISODE, after->kind);
    ASSERT_EQ(ep_start, tn_airing_start(&t, 1, after));
    ASSERT((size_t)before != (size_t)after);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

#define LOOP_CH 2
#define TEN_OH_FIVE (10u * 3600000u + 5000u)

static const ctw_date OCT_19 = { 2026, 10, 19 };
static const ctw_date OCT_20 = { 2026, 10, 20 };
static const ctw_date OCT_21 = { 2026, 10, 21 };

TEST shift_zero_matches_today(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    const ctw_channel *ch = ctw_db_channel(db, LOOP_CH);
    ASSERT(ch != NULL);
    ctw_lineup today, yesterday;
    memset(&today, 0, sizeof today);
    memset(&yesterday, 0, sizeof yesterday);
    ASSERT_EQ(0, ctw_lineup_build(db, ch, OCT_20, &today));
    ASSERT_EQ(0, ctw_lineup_build(db, ch, OCT_19, &yesterday));
    uint32_t ref_offset = 0;
    const ctw_airing *ref = ctw_lineup_at(&today, &yesterday, TEN_OH_FIVE, &ref_offset);
    ASSERT(ref != NULL);
    tuner t;
    tn_init(&t, db);
    ASSERT_EQ(0LL, tn_shift(&t, LOOP_CH));
    uint32_t offset = 0;
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(ref->start_ms, got->start_ms);
    ASSERT_EQ(ref_offset, offset);
    ASSERT_EQ(0LL, tn_shift(&t, LOOP_CH));
    tn_free(&t);
    ctw_lineup_free(&today);
    ctw_lineup_free(&yesterday);
    ctw_db_free(db);
    PASS();
}

TEST skip_next_starts_the_following_item(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    const ctw_airing *cur = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(cur != NULL);
    int64_t want = cur->start_ms + (int64_t)cur->duration_ms;
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, 1));
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(want, got->start_ms);
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST skip_prev_starts_the_item_before(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    const ctw_airing *cur = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(cur != NULL);
    ASSERT(offset > 0u);
    ASSERT(cur > t.entries[0].today.items);
    int64_t want = (cur - 1)->start_ms;
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, -1));
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(want, got->start_ms);
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST quick_presses_add_up(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    const ctw_airing *cur = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(cur != NULL);
    const ctw_airing *third = tn_next(&t, LOOP_CH, tn_next(&t, LOOP_CH, tn_next(&t, LOOP_CH, cur)));
    ASSERT(third != NULL);
    int64_t want = third->start_ms;
    for (int i = 0; i < 3; i++) ASSERT(tn_skip(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, 1));
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(want, got->start_ms);
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST skip_next_crosses_midnight(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t wall = CTW_DAY_MS - 1000u;
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, wall, 1));
    ASSERT_EQ(1000LL, tn_shift(&t, LOOP_CH));
    uint32_t offset = 1;
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, wall, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(0LL, got->start_ms);
    ASSERT_EQ(0u, offset);
    ASSERT_EQ(ctw_date_key(OCT_21), ctw_date_key(t.entries[0].day));
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST skip_prev_crosses_midnight_back(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, 5000u, -1));
    uint32_t offset = 1;
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, 5000u, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(ctw_date_key(OCT_19), ctw_date_key(t.entries[0].day));
    const ctw_lineup *l = &t.entries[0].today;
    ASSERT_EQ((size_t)&l->items[l->count - 1], (size_t)got);
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST prev_after_crossing_comes_back(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t wall = CTW_DAY_MS - 1000u;
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, wall, 1));
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, wall, -1));
    uint32_t offset = 1;
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_20, wall, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(ctw_date_key(OCT_20), ctw_date_key(t.entries[0].day));
    const ctw_lineup *l = &t.entries[0].today;
    ASSERT_EQ((size_t)&l->items[l->count - 1], (size_t)got);
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST timed_channel_never_skips(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    const ctw_airing *cur = tn_current(&t, 1, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(cur != NULL);
    ASSERT_FALSE(tn_skip(&t, 1, OCT_20, TEN_OH_FIVE, 1));
    ASSERT_FALSE(tn_skip(&t, 1, OCT_20, TEN_OH_FIVE, -1));
    ASSERT_FALSE(tn_jump(&t, 1, OCT_20, TEN_OH_FIVE, cur + 1));
    ASSERT_EQ(0LL, tn_shift(&t, 1));
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST jump_starts_the_chosen_airing(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    const ctw_airing *cur = tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset);
    ASSERT(cur != NULL);
    const ctw_lineup *l = &t.entries[0].today;
    size_t k = (size_t)(cur - l->items);
    ASSERT(k >= 3 && k + 5 < l->count);
    const ctw_airing *later = &l->items[k + 5];
    ASSERT(tn_jump(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, later));
    ASSERT_EQ((size_t)later, (size_t)tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset));
    ASSERT_EQ(0u, offset);
    const ctw_airing *earlier = &l->items[k - 3];
    ASSERT(tn_jump(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, earlier));
    ASSERT_EQ((size_t)earlier, (size_t)tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset));
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST jump_reads_target_before_rebuild(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t offset = 0;
    ASSERT(tn_current(&t, LOOP_CH, OCT_20, TEN_OH_FIVE, &offset) != NULL);
    const ctw_airing *to = &t.entries[0].today.items[0];
    ASSERT(tn_jump(&t, LOOP_CH, OCT_21, 5000u, to));
    const ctw_airing *got = tn_current(&t, LOOP_CH, OCT_21, 5000u, &offset);
    ASSERT(got != NULL);
    ASSERT_EQ(ctw_date_key(OCT_20), ctw_date_key(t.entries[0].day));
    ASSERT_EQ((size_t)t.entries[0].today.items, (size_t)got);
    ASSERT_EQ(0u, offset);
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST lineup_now_timed_after_midnight_is_yesterday(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    ctw_date day = { 0, 0, 0 };
    const ctw_lineup *l = tn_lineup_now(&t, 1, OCT_20, 2u * 3600000u, &day);
    ASSERT_EQ((size_t)&t.entries[0].yesterday, (size_t)l);
    ASSERT_EQ(ctw_date_key(OCT_19), ctw_date_key(day));
    l = tn_lineup_now(&t, 1, OCT_20, 8u * 3600000u, &day);
    ASSERT_EQ((size_t)&t.entries[0].today, (size_t)l);
    ASSERT_EQ(ctw_date_key(OCT_20), ctw_date_key(day));
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

TEST lineup_now_looping_is_the_channel_day(void)
{
    ctw_db *db = load_fixture();
    ASSERT(db != NULL);
    tuner t;
    tn_init(&t, db);
    uint32_t wall = CTW_DAY_MS - 1000u;
    ASSERT(tn_skip(&t, LOOP_CH, OCT_20, wall, 1));
    ctw_date day = { 0, 0, 0 };
    const ctw_lineup *l = tn_lineup_now(&t, LOOP_CH, OCT_20, wall, &day);
    ASSERT_EQ((size_t)&t.entries[0].today, (size_t)l);
    ASSERT_EQ(ctw_date_key(OCT_21), ctw_date_key(day));
    tn_free(&t);
    ctw_db_free(db);
    PASS();
}

SUITE(tuner_suite)
{
    RUN_TEST(shift_zero_matches_today);
    RUN_TEST(skip_next_starts_the_following_item);
    RUN_TEST(skip_prev_starts_the_item_before);
    RUN_TEST(quick_presses_add_up);
    RUN_TEST(skip_next_crosses_midnight);
    RUN_TEST(skip_prev_crosses_midnight_back);
    RUN_TEST(prev_after_crossing_comes_back);
    RUN_TEST(timed_channel_never_skips);
    RUN_TEST(jump_starts_the_chosen_airing);
    RUN_TEST(jump_reads_target_before_rebuild);
    RUN_TEST(lineup_now_timed_after_midnight_is_yesterday);
    RUN_TEST(lineup_now_looping_is_the_channel_day);
    RUN_TEST(rebuild_keeps_the_next_episode_start_across_midnight);
    RUN_TEST(next_episode_skips_ads);
    RUN_TEST(next_episode_none_at_end);
    RUN_TEST(ms_until_next_episode);
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
