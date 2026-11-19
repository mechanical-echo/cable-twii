#include "greatest/greatest.h"
#include "playback.h"

TEST end_of_file_is_eof(void)
{
    ASSERT_EQ(PB_END_EOF, pb_classify_end(false, true));
    PASS();
}

TEST read_error_is_error(void)
{
    ASSERT_EQ(PB_END_ERROR, pb_classify_end(true, true));
    PASS();
}

TEST missing_path_is_error(void)
{
    ASSERT_EQ(PB_END_ERROR, pb_classify_end(false, false));
    ASSERT_EQ(PB_END_ERROR, pb_classify_end(true, false));
    PASS();
}

TEST error_without_drive_is_disconnect(void)
{
    ASSERT_EQ(PB_DISCONNECTED, pb_on_status(PL_ERROR, 60000, false));
    ASSERT_EQ(PB_DISCONNECTED, pb_on_status(PL_ERROR, 0, false));
    PASS();
}

TEST error_with_drive_goes_black(void)
{
    ASSERT_EQ(PB_BLACK, pb_on_status(PL_ERROR, 60000, true));
    PASS();
}

TEST early_end_with_drive_stays_black(void)
{
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_ENDED, 60000, true));
    PASS();
}

TEST early_end_without_drive_is_disconnect(void)
{
    ASSERT_EQ(PB_DISCONNECTED, pb_on_status(PL_ENDED, 60000, false));
    ASSERT_EQ(PB_DISCONNECTED, pb_on_status(PL_ENDED, PB_ON_TIME_MS + 1, false));
    PASS();
}

TEST on_time_end_is_normal(void)
{
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_ENDED, 0, false));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_ENDED, PB_ON_TIME_MS, false));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_ENDED, 0, true));
    PASS();
}

TEST normal_states_keep(void)
{
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_PLAYING, 60000, false));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_SEEKING, 60000, true));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_IDLE, 60000, false));
    PASS();
}

TEST probe_only_on_error_or_early_end(void)
{
    ASSERT(pb_needs_probe(PL_ERROR, 0));
    ASSERT(pb_needs_probe(PL_ERROR, 60000));
    ASSERT(pb_needs_probe(PL_ENDED, 60000));
    ASSERT(!pb_needs_probe(PL_ENDED, PB_ON_TIME_MS));
    ASSERT(!pb_needs_probe(PL_ENDED, 0));
    ASSERT(!pb_needs_probe(PL_PLAYING, 60000));
    ASSERT(!pb_needs_probe(PL_IDLE, 60000));
    PASS();
}

TEST stall_note_after_a_second(void)
{
    ASSERT_EQ(PB_STALL_NOTE, pb_stall_action(PL_PLAYING, 1000, false, 0, 0));
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_PLAYING, 999, false, 0, 0));
    PASS();
}

TEST stall_rejoins_when_far_behind(void)
{
    ASSERT_EQ(PB_STALL_REJOIN, pb_stall_action(PL_PLAYING, 0, true, 10000, 12500));
    PASS();
}

TEST stall_no_rejoin_when_close(void)
{
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_PLAYING, 0, true, 10000, 11500));
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_PLAYING, 0, true, 10000, 10000 + PB_REJOIN_LAG_MS));
    PASS();
}

TEST stall_keeps_note_until_data_flows(void)
{
    ASSERT_EQ(PB_STALL_NOTE, pb_stall_action(PL_PLAYING, PB_NOTE_MS, true, 10000, 30000));
    PASS();
}

TEST stall_no_rejoin_without_a_stall(void)
{
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_PLAYING, 0, false, 10000, 30000));
    PASS();
}

TEST stall_no_rejoin_when_ahead(void)
{
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_PLAYING, 0, true, 12500, 10000));
    PASS();
}

TEST lost_on_error_or_long_wait(void)
{
    ASSERT(pb_lost(PL_ERROR, 0));
    ASSERT(pb_lost(PL_PLAYING, PB_GIVEUP_MS));
    ASSERT(!pb_lost(PL_PLAYING, PB_GIVEUP_MS - 1));
    ASSERT(!pb_lost(PL_PLAYING, 0));
    PASS();
}

TEST lost_counts_waiting_while_playing_or_seeking(void)
{
    ASSERT(pb_lost(PL_SEEKING, PB_GIVEUP_MS));
    ASSERT(!pb_lost(PL_SEEKING, PB_GIVEUP_MS - 1));
    ASSERT(!pb_lost(PL_ENDED, 60000));
    ASSERT(!pb_lost(PL_IDLE, 60000));
    PASS();
}

TEST waiting_counts_while_playing_or_seeking(void)
{
    ASSERT_EQ(5000u, pb_play_waiting(PL_SEEKING, 5000, 0));
    ASSERT_EQ(0u, pb_play_waiting(PL_ERROR, 5000, 5000));
    ASSERT_EQ(0u, pb_play_waiting(PL_IDLE, 5000, 5000));
    ASSERT_EQ(1500u, pb_play_waiting(PL_PLAYING, 1500, 60000));
    PASS();
}

TEST stall_note_while_seeking(void)
{
    ASSERT_EQ(PB_STALL_NOTE, pb_stall_action(PL_SEEKING, PB_NOTE_MS, false, 0, 0));
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_SEEKING, PB_NOTE_MS - 1, false, 0, 0));
    PASS();
}

TEST seeking_never_rejoins(void)
{
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_SEEKING, 0, true, 0, 30000));
    PASS();
}

TEST waiting_from_the_tune_is_not_a_stall(void)
{
    ASSERT_EQ(400u, pb_play_waiting(PL_PLAYING, 1500, 400));
    ASSERT_EQ(PB_STALL_NONE, pb_stall_action(PL_PLAYING, pb_play_waiting(PL_PLAYING, 1500, 400), false, 10000, 12500));
    PASS();
}

TEST server_probe_first_at_once_then_spaced(void)
{
    ASSERT(pb_probe_due(false, 0));
    ASSERT(!pb_probe_due(true, 0));
    ASSERT(!pb_probe_due(true, PB_PROBE_MS - 1));
    ASSERT(pb_probe_due(true, PB_PROBE_MS));
    PASS();
}

SUITE(playback_suite)
{
    RUN_TEST(end_of_file_is_eof);
    RUN_TEST(read_error_is_error);
    RUN_TEST(missing_path_is_error);
    RUN_TEST(error_without_drive_is_disconnect);
    RUN_TEST(error_with_drive_goes_black);
    RUN_TEST(early_end_with_drive_stays_black);
    RUN_TEST(early_end_without_drive_is_disconnect);
    RUN_TEST(on_time_end_is_normal);
    RUN_TEST(normal_states_keep);
    RUN_TEST(probe_only_on_error_or_early_end);
    RUN_TEST(stall_note_after_a_second);
    RUN_TEST(stall_rejoins_when_far_behind);
    RUN_TEST(stall_no_rejoin_when_close);
    RUN_TEST(stall_keeps_note_until_data_flows);
    RUN_TEST(stall_no_rejoin_without_a_stall);
    RUN_TEST(stall_no_rejoin_when_ahead);
    RUN_TEST(lost_on_error_or_long_wait);
    RUN_TEST(lost_counts_waiting_while_playing_or_seeking);
    RUN_TEST(waiting_counts_while_playing_or_seeking);
    RUN_TEST(stall_note_while_seeking);
    RUN_TEST(seeking_never_rejoins);
    RUN_TEST(waiting_from_the_tune_is_not_a_stall);
    RUN_TEST(server_probe_first_at_once_then_spaced);
}
