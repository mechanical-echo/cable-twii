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
    ASSERT_EQ(PB_DISCONNECTED, pb_on_status(PL_ERROR, false));
    PASS();
}

TEST error_with_drive_goes_black(void)
{
    ASSERT_EQ(PB_BLACK, pb_on_status(PL_ERROR, true));
    PASS();
}

TEST short_file_keeps_airing(void)
{
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_ENDED, true));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_ENDED, false));
    PASS();
}

TEST normal_states_keep(void)
{
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_PLAYING, false));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_SEEKING, true));
    ASSERT_EQ(PB_KEEP, pb_on_status(PL_IDLE, false));
    PASS();
}

SUITE(playback_suite)
{
    RUN_TEST(end_of_file_is_eof);
    RUN_TEST(read_error_is_error);
    RUN_TEST(missing_path_is_error);
    RUN_TEST(error_without_drive_is_disconnect);
    RUN_TEST(error_with_drive_goes_black);
    RUN_TEST(short_file_keeps_airing);
    RUN_TEST(normal_states_keep);
}
