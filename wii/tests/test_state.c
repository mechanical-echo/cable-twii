#include <stdio.h>
#include <string.h>
#include "greatest/greatest.h"
#include "state.h"

#define DIR "build/state"

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) return;
    fputs(text, f);
    fclose(f);
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;
    fclose(f);
    return true;
}

static void clean_dir(void)
{
    remove(DIR "/state.toml");
    remove(DIR "/state.toml.tmp");
}

TEST state_round_trip(void)
{
    ctw_state in = { 3, 60 };
    ctw_state out = { 0, 0 };
    char buf[64];
    size_t n = state_format(&in, buf, sizeof buf);
    ASSERT_EQ(strlen(buf), n);
    ASSERT_STR_EQ("last_channel = 3\nvolume = 60\n", buf);
    ASSERT_EQ(0, state_parse(buf, &out));
    ASSERT_EQ(3, out.last_channel);
    ASSERT_EQ(60, out.volume);
    PASS();
}

TEST state_defaults(void)
{
    ctw_state out = { 9, 9 };
    ASSERT_EQ(0, state_parse("", &out));
    ASSERT_EQ(0, out.last_channel);
    ASSERT_EQ(80, out.volume);
    PASS();
}

TEST state_clamps(void)
{
    ctw_state out = { 9, 9 };
    ASSERT_EQ(0, state_parse("volume = 250", &out));
    ASSERT_EQ(100, out.volume);
    ASSERT_EQ(0, out.last_channel);
    ASSERT_EQ(0, state_parse("volume = -4\n", &out));
    ASSERT_EQ(0, out.volume);
    PASS();
}

TEST state_garbage(void)
{
    ctw_state out = { 9, 9 };
    ASSERT_EQ(-1, state_parse("%%%", &out));
    ASSERT_EQ(0, out.last_channel);
    ASSERT_EQ(80, out.volume);
    PASS();
}

TEST state_wrong_types_are_broken(void)
{
    ctw_state out = { 9, 9 };
    ASSERT_EQ(-1, state_parse("last_channel = \"two\"\nvolume = 30\n", &out));
    ASSERT_EQ(0, out.last_channel);
    ASSERT_EQ(80, out.volume);
    ASSERT_EQ(-1, state_parse("last_channel = 2\nvolume = 1.5\n", &out));
    ASSERT_EQ(80, out.volume);
    PASS();
}

TEST state_accepts_bom_crlf_and_unknown_keys(void)
{
    ctw_state out = { 9, 9 };
    ASSERT_EQ(0, state_parse("\xEF\xBB\xBFlast_channel = 12\r\nvolume = 5\r\nextra = true\r\n", &out));
    ASSERT_EQ(12, out.last_channel);
    ASSERT_EQ(5, out.volume);
    PASS();
}

TEST state_format_too_small(void)
{
    ctw_state in = { 3, 60 };
    char buf[8];
    ASSERT_EQ(0u, state_format(&in, buf, sizeof buf));
    PASS();
}

TEST state_save_then_load(void)
{
    clean_dir();
    ctw_state in = { 7, 33 };
    ctw_state out = { 0, 0 };
    state_save(DIR, &in);
    ASSERT(file_exists(DIR "/state.toml"));
    ASSERT(!file_exists(DIR "/state.toml.tmp"));
    state_load(DIR, &out);
    ASSERT_EQ(7, out.last_channel);
    ASSERT_EQ(33, out.volume);

    ctw_state again = { 2, 90 };
    state_save(DIR, &again);
    state_load(DIR, &out);
    ASSERT_EQ(2, out.last_channel);
    ASSERT_EQ(90, out.volume);
    clean_dir();
    PASS();
}

TEST state_load_missing_or_broken_gives_defaults(void)
{
    clean_dir();
    ctw_state out = { 9, 9 };
    state_load(DIR, &out);
    ASSERT_EQ(0, out.last_channel);
    ASSERT_EQ(80, out.volume);
    write_file(DIR "/state.toml", "volume = [");
    out.volume = 9;
    state_load(DIR, &out);
    ASSERT_EQ(80, out.volume);
    clean_dir();
    PASS();
}

TEST state_load_falls_back_to_tmp(void)
{
    clean_dir();
    write_file(DIR "/state.toml.tmp", "last_channel = 4\nvolume = 20\n");
    ctw_state out = { 0, 0 };
    state_load(DIR, &out);
    ASSERT_EQ(4, out.last_channel);
    ASSERT_EQ(20, out.volume);
    write_file(DIR "/state.toml", "last_channel = 5\nvolume = 21\n");
    state_load(DIR, &out);
    ASSERT_EQ(5, out.last_channel);
    clean_dir();
    PASS();
}

TEST state_save_into_missing_folder_does_nothing(void)
{
    ctw_state in = { 1, 2 };
    state_save(DIR "/missing", &in);
    ASSERT(!file_exists(DIR "/missing/state.toml"));
    ASSERT(!file_exists(DIR "/missing/state.toml.tmp"));
    PASS();
}

TEST state_changed_compares_both_fields(void)
{
    ctw_state a = { 1, 80 };
    ctw_state b = { 1, 80 };
    ASSERT(!state_changed(&a, &b));
    b.volume = 75;
    ASSERT(state_changed(&a, &b));
    b.volume = 80;
    b.last_channel = 2;
    ASSERT(state_changed(&a, &b));
    PASS();
}

TEST state_save_throttled_to_ten_seconds(void)
{
    ASSERT(!state_save_due(false, 0, 60000));
    ASSERT(!state_save_due(true, 1000, 1000));
    ASSERT(!state_save_due(true, 1000, 10999));
    ASSERT(state_save_due(true, 1000, 11000));
    ASSERT(state_save_due(true, 1000, 50000));
    ASSERT(!state_save_due(true, 0xFFFFF000u, 0x00000100u));
    ASSERT(state_save_due(true, 0xFFFFF000u, 0x00002000u + 10000u));
    PASS();
}

TEST state_start_channel_uses_last_if_known(void)
{
    int nums[] = { 2, 5, 9 };
    ASSERT_EQ(5, state_start_channel(5, nums, 3));
    ASSERT_EQ(2, state_start_channel(7, nums, 3));
    ASSERT_EQ(2, state_start_channel(0, nums, 3));
    ASSERT_EQ(0, state_start_channel(5, NULL, 0));
    PASS();
}

SUITE(state_suite)
{
    RUN_TEST(state_round_trip);
    RUN_TEST(state_defaults);
    RUN_TEST(state_clamps);
    RUN_TEST(state_garbage);
    RUN_TEST(state_wrong_types_are_broken);
    RUN_TEST(state_accepts_bom_crlf_and_unknown_keys);
    RUN_TEST(state_format_too_small);
    RUN_TEST(state_save_then_load);
    RUN_TEST(state_load_missing_or_broken_gives_defaults);
    RUN_TEST(state_load_falls_back_to_tmp);
    RUN_TEST(state_save_into_missing_folder_does_nothing);
    RUN_TEST(state_changed_compares_both_fields);
    RUN_TEST(state_save_throttled_to_ten_seconds);
    RUN_TEST(state_start_channel_uses_last_if_known);
}
