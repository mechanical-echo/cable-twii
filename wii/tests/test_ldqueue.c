#include "greatest/greatest.h"
#include "ldqueue.h"

static lq_state empty(void)
{
    lq_state q = { false, false, false, LQ_NONE };
    return q;
}

TEST nothing_wanted_starts_nothing(void)
{
    lq_state q = empty();
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST a_busy_loader_defers_the_request(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, false));
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, false));
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, false));
    PASS();
}

TEST a_check_waits_for_a_busy_loader_too(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, true));
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, true));
    PASS();
}

TEST load_comes_first_and_covers_a_waiting_check(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, true));
    ASSERT_EQ(LQ_LOAD, lq_finish(&q));
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST check_comes_before_the_probe(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, true));
    ASSERT_EQ(LQ_CHECK, lq_finish(&q));
    ASSERT_EQ(LQ_PROBE, lq_start(&q, false, true));
    PASS();
}

TEST probe_starts_only_when_due(void)
{
    lq_state q = empty();
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    ASSERT_EQ(LQ_PROBE, lq_start(&q, false, true));
    PASS();
}

TEST a_check_wanted_during_a_load_runs_after_it(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, false));
    lq_want(&q, LQ_CHECK);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, false));
    ASSERT_EQ(LQ_LOAD, lq_finish(&q));
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, false));
    PASS();
}

TEST a_load_wanted_during_a_probe_runs_after_it(void)
{
    lq_state q = empty();
    ASSERT_EQ(LQ_PROBE, lq_start(&q, false, true));
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, true));
    ASSERT_EQ(LQ_PROBE, lq_finish(&q));
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, true));
    PASS();
}

TEST finish_hands_the_result_to_its_owner_once(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, false));
    ASSERT_EQ(LQ_CHECK, lq_finish(&q));
    ASSERT_EQ(LQ_NONE, lq_finish(&q));
    PASS();
}

TEST nothing_starts_until_the_result_is_handed_over(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, false));
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, true));
    ASSERT_EQ(LQ_CHECK, lq_finish(&q));
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, true));
    PASS();
}

TEST a_wanted_job_runs_once(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    lq_want(&q, LQ_CHECK);
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, false));
    ASSERT_EQ(LQ_CHECK, lq_finish(&q));
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST reset_orphans_the_running_job(void)
{
    lq_job jobs[] = { LQ_LOAD, LQ_CHECK };
    for (size_t i = 0; i < sizeof jobs / sizeof jobs[0]; i++) {
        lq_state q = empty();
        lq_want(&q, jobs[i]);
        ASSERT_EQ(jobs[i], lq_start(&q, false, false));
        lq_reset(&q);
        ASSERT_EQ(LQ_NONE, lq_finish(&q));
    }
    lq_state q = empty();
    ASSERT_EQ(LQ_PROBE, lq_start(&q, false, true));
    lq_reset(&q);
    ASSERT_EQ(LQ_NONE, lq_finish(&q));
    PASS();
}

TEST reset_forgets_waiting_requests(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_LOAD);
    lq_want(&q, LQ_CHECK);
    lq_reset(&q);
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST a_load_wanted_after_reset_waits_for_the_orphan(void)
{
    lq_state q = empty();
    ASSERT_EQ(LQ_PROBE, lq_start(&q, false, true));
    lq_reset(&q);
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, false));
    ASSERT_EQ(LQ_NONE, lq_finish(&q));
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, false));
    PASS();
}

TEST a_test_runs_when_the_loader_is_free(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, false));
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    ASSERT_EQ(LQ_TEST, lq_finish(&q));
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST a_load_comes_before_a_test_and_the_test_still_runs(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, false));
    ASSERT_EQ(LQ_LOAD, lq_finish(&q));
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    PASS();
}

TEST a_test_comes_before_a_check_and_the_probe(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_CHECK);
    lq_want(&q, LQ_TEST);
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, true));
    ASSERT_EQ(LQ_TEST, lq_finish(&q));
    ASSERT_EQ(LQ_CHECK, lq_start(&q, false, true));
    PASS();
}

TEST dropping_a_running_test_orphans_it(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    lq_drop(&q, LQ_TEST);
    ASSERT_EQ(LQ_NONE, lq_finish(&q));
    PASS();
}

TEST dropping_a_waiting_test_forgets_it(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    lq_drop(&q, LQ_TEST);
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST a_new_test_waits_for_the_dropped_one(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    lq_drop(&q, LQ_TEST);
    lq_want(&q, LQ_TEST);
    ASSERT_EQ(LQ_NONE, lq_start(&q, true, false));
    ASSERT_EQ(LQ_NONE, lq_finish(&q));
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    ASSERT_EQ(LQ_TEST, lq_finish(&q));
    PASS();
}

TEST dropping_a_test_leaves_the_running_load(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_LOAD);
    ASSERT_EQ(LQ_LOAD, lq_start(&q, false, false));
    lq_drop(&q, LQ_TEST);
    ASSERT_EQ(LQ_LOAD, lq_finish(&q));
    PASS();
}

TEST reset_keeps_a_waiting_test(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    lq_want(&q, LQ_CHECK);
    lq_reset(&q);
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    ASSERT_EQ(LQ_TEST, lq_finish(&q));
    ASSERT_EQ(LQ_NONE, lq_start(&q, false, false));
    PASS();
}

TEST reset_keeps_a_running_test(void)
{
    lq_state q = empty();
    lq_want(&q, LQ_TEST);
    ASSERT_EQ(LQ_TEST, lq_start(&q, false, false));
    lq_reset(&q);
    ASSERT_EQ(LQ_TEST, lq_finish(&q));
    PASS();
}

TEST new_day_ignores_the_first_day_seen(void)
{
    uint32_t seen = 0;
    ASSERT_FALSE(lq_new_day(&seen, 20261119u));
    ASSERT_FALSE(lq_new_day(&seen, 20261119u));
    PASS();
}

TEST new_day_fires_once_per_change(void)
{
    uint32_t seen = 0;
    lq_new_day(&seen, 20261119u);
    ASSERT(lq_new_day(&seen, 20261120u));
    ASSERT_FALSE(lq_new_day(&seen, 20261120u));
    ASSERT(lq_new_day(&seen, 20261121u));
    PASS();
}

TEST new_day_starts_over_without_a_day(void)
{
    uint32_t seen = 0;
    lq_new_day(&seen, 20261119u);
    ASSERT_FALSE(lq_new_day(&seen, 0));
    ASSERT_FALSE(lq_new_day(&seen, 20261120u));
    ASSERT_FALSE(lq_new_day(&seen, 20261120u));
    PASS();
}

SUITE(ldqueue_suite)
{
    RUN_TEST(nothing_wanted_starts_nothing);
    RUN_TEST(a_busy_loader_defers_the_request);
    RUN_TEST(a_check_waits_for_a_busy_loader_too);
    RUN_TEST(load_comes_first_and_covers_a_waiting_check);
    RUN_TEST(check_comes_before_the_probe);
    RUN_TEST(probe_starts_only_when_due);
    RUN_TEST(a_check_wanted_during_a_load_runs_after_it);
    RUN_TEST(a_load_wanted_during_a_probe_runs_after_it);
    RUN_TEST(finish_hands_the_result_to_its_owner_once);
    RUN_TEST(nothing_starts_until_the_result_is_handed_over);
    RUN_TEST(a_wanted_job_runs_once);
    RUN_TEST(reset_orphans_the_running_job);
    RUN_TEST(reset_forgets_waiting_requests);
    RUN_TEST(a_load_wanted_after_reset_waits_for_the_orphan);
    RUN_TEST(a_test_runs_when_the_loader_is_free);
    RUN_TEST(a_load_comes_before_a_test_and_the_test_still_runs);
    RUN_TEST(a_test_comes_before_a_check_and_the_probe);
    RUN_TEST(dropping_a_running_test_orphans_it);
    RUN_TEST(dropping_a_waiting_test_forgets_it);
    RUN_TEST(a_new_test_waits_for_the_dropped_one);
    RUN_TEST(dropping_a_test_leaves_the_running_load);
    RUN_TEST(reset_keeps_a_waiting_test);
    RUN_TEST(reset_keeps_a_running_test);
    RUN_TEST(new_day_ignores_the_first_day_seen);
    RUN_TEST(new_day_fires_once_per_change);
    RUN_TEST(new_day_starts_over_without_a_day);
}
