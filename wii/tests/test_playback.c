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
}
