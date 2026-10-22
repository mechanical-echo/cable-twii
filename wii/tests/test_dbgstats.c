#include <string.h>

#include "greatest/greatest.h"
#include "debug/dbgstats.h"

TEST rate_waits_for_a_full_window(void)
{
    dbg_rate r;
    dbg_rate_init(&r);
    ASSERT_FALSE(dbg_rate_update(&r, 5000000, 0));
    ASSERT_FALSE(dbg_rate_update(&r, 5999999, 59));
    ASSERT_IN_RANGE(0.0f, r.rate, 0.001f);
    PASS();
}

TEST rate_counts_per_second_of_real_time(void)
{
    dbg_rate r;
    dbg_rate_init(&r);
    dbg_rate_update(&r, 1000000, 100);
    ASSERT(dbg_rate_update(&r, 2000000, 160));
    ASSERT_IN_RANGE(60.0f, r.rate, 0.001f);
    PASS();
}

TEST rate_uses_elapsed_time_not_frame_count(void)
{
    dbg_rate r;
    dbg_rate_init(&r);
    dbg_rate_update(&r, 0, 0);
    ASSERT(dbg_rate_update(&r, 2000000, 60));
    ASSERT_IN_RANGE(30.0f, r.rate, 0.001f);
    PASS();
}

TEST rate_starts_a_new_window(void)
{
    dbg_rate r;
    dbg_rate_init(&r);
    dbg_rate_update(&r, 0, 0);
    dbg_rate_update(&r, 1000000, 60);
    ASSERT_FALSE(dbg_rate_update(&r, 1500000, 75));
    ASSERT_IN_RANGE(60.0f, r.rate, 0.001f);
    ASSERT(dbg_rate_update(&r, 2000000, 90));
    ASSERT_IN_RANGE(30.0f, r.rate, 0.001f);
    PASS();
}

TEST rate_survives_counter_wrap(void)
{
    dbg_rate r;
    dbg_rate_init(&r);
    dbg_rate_update(&r, 0, 0xFFFFFFF0u);
    ASSERT(dbg_rate_update(&r, 1000000, 0x0000000Cu));
    ASSERT_IN_RANGE(28.0f, r.rate, 0.001f);
    PASS();
}

TEST format_lines(void)
{
    dbg_stats s;
    memset(&s, 0, sizeof s);
    s.ui_fps = 59.94f;
    s.video_fps = 29.97f;
    s.underruns = 3;
    s.decoded = 12345;
    s.dropped = 7;
    s.mem1_free = 1536u * 1024u;
    s.mem2_free = 40000u * 1024u;
    s.heap_used = 9000u * 1024u;
    s.heap_free = 512u * 1024u;
    char lines[DBG_LINES][DBG_LINE_BYTES];
    dbg_format(&s, lines);
    ASSERT_STR_EQ("UI 59.9 fps  video 30.0 fps", lines[0]);
    ASSERT_STR_EQ("audio underruns 3", lines[1]);
    ASSERT_STR_EQ("frames 12345 decoded  7 dropped", lines[2]);
    ASSERT_STR_EQ("MEM1 1536K  MEM2 40000K free", lines[3]);
    ASSERT_STR_EQ("heap 9000K used  512K free", lines[4]);
    PASS();
}

TEST format_rounds_memory_down_to_kilobytes(void)
{
    dbg_stats s;
    memset(&s, 0, sizeof s);
    s.mem1_free = 2047;
    s.mem2_free = 1024;
    char lines[DBG_LINES][DBG_LINE_BYTES];
    dbg_format(&s, lines);
    ASSERT_STR_EQ("MEM1 1K  MEM2 1K free", lines[3]);
    ASSERT_STR_EQ("UI 0.0 fps  video 0.0 fps", lines[0]);
    PASS();
}

SUITE(dbgstats_suite)
{
    RUN_TEST(rate_waits_for_a_full_window);
    RUN_TEST(rate_counts_per_second_of_real_time);
    RUN_TEST(rate_uses_elapsed_time_not_frame_count);
    RUN_TEST(rate_starts_a_new_window);
    RUN_TEST(rate_survives_counter_wrap);
    RUN_TEST(format_lines);
    RUN_TEST(format_rounds_memory_down_to_kilobytes);
}
