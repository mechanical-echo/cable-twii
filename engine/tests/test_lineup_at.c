#include <stdint.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"
#include "helpers.h"

#define AT_DAY_MS ((int64_t)CTW_DAY_MS)

#define AT_TIMED(seed) \
    "version = 1\nseed = " seed "\n\n[[channel]]\nnumber = 1\nname = \"Cartoon Network\"\n" \
    "category = \"Kids\"\nads = true\nmode = \"timed\"\n"

#define AT_SLOT(at, items) "[[channel.slot]]\nat = \"" at "\"\nitems = [ " items " ]\n"

#define AT_FILL "{ type = \"fill\", show = \"Gumball\", ads_between = 2 }"

#define AT_PINNED \
    AT_TIMED("1") \
    AT_SLOT("07:00", \
        "{ type = \"random-run\", show = \"Gumball\", count = 3 }, " \
        "{ type = \"ads\", count = 1 }, " \
        "{ type = \"random\", show = \"Adventure Time\", season = 5, count = 5 }, " \
        "{ type = \"ads\", count = 2 }, " \
        "{ type = \"episode\", show = \"Ben 10\", season = 1, episode = 5 }, " \
        "{ type = \"random\", show = \"Gumball\", count = 2 }") \
    AT_SLOT("19:00", "{ type = \"fill\", show = \"Adventure Time\", ads_between = 2 }")

#define AT_TWO_SLOTS AT_TIMED("7") AT_SLOT("07:00", AT_FILL) AT_SLOT("19:00", AT_FILL)

#define AT_ONE_SLOT AT_TIMED("7") AT_SLOT("07:00", AT_FILL)

#define AT_LOOP \
    "version = 1\nseed = 1\n\n[[channel]]\nnumber = 2\nname = \"MTV\"\n" \
    "category = \"Music\"\nfolder = \"mtv-00s\"\nads = false\nmode = \"looping\"\n" \
    "loop = [ { type = \"random\", show = \"Music Videos\", count = 10 } ]\n"

static const ctw_date at_day = {2026, 10, 20};

static int build_pair(const char *cfg, ctw_lineup *today, ctw_lineup *yesterday)
{
    if (build_lineup(cfg, 1, at_day, today)) return -1;
    if (build_lineup(cfg, 1, ctw_date_prev(at_day), yesterday)) {
        ctw_lineup_free(today);
        return -1;
    }
    return 0;
}

static const ctw_airing *linear_at(const ctw_lineup *l, int64_t t)
{
    for (size_t i = 0; i < l->count; i++) {
        const ctw_airing *a = &l->items[i];
        if (a->start_ms <= t && t < a->start_ms + (int64_t)a->duration_ms) return a;
    }
    return NULL;
}

TEST at_offset_inside(void)
{
    ctw_lineup l = {0};
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_PINNED, 1, at_day, &l));
    ASSERT_EQ(&l.items[0], ctw_lineup_at(&l, NULL, 25205000u, &off));
    ASSERT_EQ(5000u, off);
    ctw_lineup_free(&l);
    PASS();
}

TEST at_exact_boundary(void)
{
    ctw_lineup l = {0};
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_PINNED, 1, at_day, &l));
    ASSERT_EQ(&l.items[1], ctw_lineup_at(&l, NULL, (uint32_t)l.items[1].start_ms, &off));
    ASSERT_EQ(0u, off);
    ASSERT_EQ(&l.items[0], ctw_lineup_at(&l, NULL, (uint32_t)l.items[1].start_ms - 1u, &off));
    ASSERT_EQ(l.items[0].duration_ms - 1u, off);
    ctw_lineup_free(&l);
    PASS();
}

TEST at_spill_from_yesterday(void)
{
    ctw_lineup today = {0};
    ctw_lineup yest = {0};
    const ctw_airing *a;
    uint32_t off = 99;
    ASSERT_EQ(0, build_pair(AT_TWO_SLOTS, &today, &yest));
    a = ctw_lineup_at(&today, &yest, 10800000u, &off);
    ASSERT(a);
    ASSERT(a >= yest.items && a < yest.items + yest.count);
    ASSERT(a->start_ms - AT_DAY_MS <= 10800000);
    ASSERT(10800000 < a->start_ms - AT_DAY_MS + (int64_t)a->duration_ms);
    ASSERT_EQ_FMT(10800000 - (a->start_ms - AT_DAY_MS), (int64_t)off, "%lld");
    ctw_lineup_free(&today);
    ctw_lineup_free(&yest);
    PASS();
}

TEST at_looping_midnight(void)
{
    ctw_lineup l = {0};
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_LOOP, 2, at_day, &l));
    ASSERT_EQ(&l.items[0], ctw_lineup_at(&l, NULL, 0u, &off));
    ASSERT_EQ(0u, off);
    ctw_lineup_free(&l);
    PASS();
}

TEST at_next(void)
{
    ctw_lineup l = {0};
    ASSERT_EQ(0, build_lineup(AT_PINNED, 1, at_day, &l));
    ASSERT_EQ(&l.items[1], ctw_lineup_next(&l, &l.items[0]));
    ASSERT_EQ(NULL, ctw_lineup_next(&l, &l.items[l.count - 1]));
    ASSERT_EQ(&l.items[l.count - 1], ctw_lineup_next(&l, &l.items[l.count - 2]));
    ctw_lineup_free(&l);
    PASS();
}

TEST at_next_invalid(void)
{
    ctw_lineup l = {0};
    ctw_lineup other = {0};
    ASSERT_EQ(0, build_lineup(AT_PINNED, 1, at_day, &l));
    ASSERT_EQ(0, build_lineup(AT_PINNED, 1, at_day, &other));
    ASSERT_EQ(NULL, ctw_lineup_next(&l, NULL));
    ASSERT_EQ(NULL, ctw_lineup_next(NULL, &l.items[0]));
    ASSERT_EQ(NULL, ctw_lineup_next(&l, &other.items[0]));
    ASSERT_EQ(NULL, ctw_lineup_next(&l, l.items + l.count));
    ctw_lineup_free(&l);
    ctw_lineup_free(&other);
    PASS();
}

TEST at_before_first_no_yesterday(void)
{
    ctw_lineup l = {0};
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_ONE_SLOT, 1, at_day, &l));
    ASSERT_EQ(NULL, ctw_lineup_at(&l, NULL, 10800000u, &off));
    ASSERT_EQ(0u, off);
    ctw_lineup_free(&l);
    PASS();
}

TEST at_last_ms_of_day(void)
{
    ctw_lineup l = {0};
    const ctw_airing *last;
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_LOOP, 2, at_day, &l));
    last = &l.items[l.count - 1];
    ASSERT_EQ(last, ctw_lineup_at(&l, NULL, CTW_DAY_MS - 1u, &off));
    ASSERT_EQ(last->duration_ms - 1u, off);
    ctw_lineup_free(&l);
    PASS();
}

TEST at_spill_edges(void)
{
    ctw_lineup today = {0};
    ctw_lineup yest = {0};
    const ctw_airing *a;
    uint32_t off = 99;
    ASSERT_EQ(0, build_pair(AT_TWO_SLOTS, &today, &yest));
    a = ctw_lineup_at(&today, &yest, 25200000u, &off);
    ASSERT_EQ(&today.items[0], a);
    ASSERT_EQ(0u, off);
    a = ctw_lineup_at(&today, &yest, 25199999u, &off);
    ASSERT(a);
    ASSERT(a >= yest.items && a < yest.items + yest.count);
    ASSERT_EQ_FMT(25199999 + AT_DAY_MS, a->start_ms + (int64_t)off, "%lld");
    ASSERT_EQ(a->duration_ms - 1u, off);
    ctw_lineup_free(&today);
    ctw_lineup_free(&yest);
    PASS();
}

TEST at_out_of_range(void)
{
    ctw_lineup l = {0};
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_LOOP, 2, at_day, &l));
    ASSERT_EQ(NULL, ctw_lineup_at(&l, NULL, CTW_DAY_MS, &off));
    ASSERT_EQ(0u, off);
    off = 99;
    ASSERT_EQ(NULL, ctw_lineup_at(&l, NULL, UINT32_MAX, &off));
    ASSERT_EQ(0u, off);
    ctw_lineup_free(&l);
    PASS();
}

TEST at_null_arguments(void)
{
    ctw_lineup l = {0};
    uint32_t off = 99;
    ASSERT_EQ(0, build_lineup(AT_LOOP, 2, at_day, &l));
    ASSERT_EQ(NULL, ctw_lineup_at(NULL, NULL, 0u, &off));
    ASSERT_EQ(0u, off);
    ASSERT_EQ(NULL, ctw_lineup_at(NULL, &l, 0u, NULL));
    ASSERT(ctw_lineup_at(&l, NULL, 1000u, NULL));
    ctw_lineup_free(&l);
    PASS();
}

TEST at_matches_linear_scan(void)
{
    ctw_lineup today = {0};
    ctw_lineup yest = {0};
    ASSERT_EQ(0, build_pair(AT_PINNED, &today, &yest));
    for (uint32_t i = 0; i < 500u; i++) {
        uint32_t ms = (uint32_t)((uint64_t)i * CTW_DAY_MS / 500u);
        const ctw_airing *expect = linear_at(&today, ms);
        uint32_t off = 99;
        const ctw_airing *got;
        int64_t base;
        if (!expect) expect = linear_at(&yest, (int64_t)ms + AT_DAY_MS);
        base = expect ? expect->start_ms : 0;
        got = ctw_lineup_at(&today, &yest, ms, &off);
        ASSERT_EQ(expect, got);
        if (expect) {
            int64_t t = linear_at(&today, ms) ? (int64_t)ms : (int64_t)ms + AT_DAY_MS;
            ASSERT_EQ_FMT(t - base, (int64_t)off, "%lld");
        }
    }
    ctw_lineup_free(&today);
    ctw_lineup_free(&yest);
    PASS();
}

SUITE(at_suite)
{
    RUN_TEST(at_offset_inside);
    RUN_TEST(at_exact_boundary);
    RUN_TEST(at_spill_from_yesterday);
    RUN_TEST(at_looping_midnight);
    RUN_TEST(at_next);
    RUN_TEST(at_next_invalid);
    RUN_TEST(at_before_first_no_yesterday);
    RUN_TEST(at_last_ms_of_day);
    RUN_TEST(at_spill_edges);
    RUN_TEST(at_out_of_range);
    RUN_TEST(at_null_arguments);
    RUN_TEST(at_matches_linear_scan);
}
