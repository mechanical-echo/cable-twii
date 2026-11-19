#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "greatest/greatest.h"
#include "settings.h"

#define DIR "build/state"

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return false;
    fclose(f);
    return true;
}

static void clean_dir(void)
{
    remove(DIR "/settings.toml");
    remove(DIR "/settings.toml.tmp");
}

TEST settings_round_trip(void)
{
    ctw_settings in = { SRC_SERVER, "192.168.1.20:4747", ASP_14_9 };
    ctw_settings out = { SRC_DRIVE, "", ASP_FIT };
    char buf[256];
    size_t n = settings_format(&in, buf, sizeof buf);
    ASSERT_EQ(strlen(buf), n);
    ASSERT_STR_EQ("source = \"server\"\nserver = \"192.168.1.20:4747\"\npicture = \"14:9\"\n", buf);
    ASSERT_EQ(0, settings_parse(buf, &out));
    ASSERT_EQ(SRC_SERVER, out.source);
    ASSERT_STR_EQ("192.168.1.20:4747", out.server);
    ASSERT_EQ(ASP_14_9, out.picture);
    PASS();
}

TEST settings_defaults_when_missing(void)
{
    ctw_settings s = { SRC_SERVER, "old", ASP_FIT };
    settings_load(DIR "/none", &s);
    ASSERT_EQ(SRC_DRIVE, s.source);
    ASSERT_STR_EQ("", s.server);
    PASS();
}

TEST settings_unknown_source_is_drive(void)
{
    ctw_settings s = { SRC_SERVER, "old", ASP_FIT };
    ASSERT_EQ(0, settings_parse("source = \"usb\"\nserver = \"pc:4747\"\n", &s));
    ASSERT_EQ(SRC_DRIVE, s.source);
    ASSERT_STR_EQ("pc:4747", s.server);
    ASSERT_EQ(0, settings_parse("server = \"pc\"\n", &s));
    ASSERT_EQ(SRC_DRIVE, s.source);
    ASSERT_EQ(0, settings_parse("\xEF\xBB\xBFsource = \"drive\"\r\n", &s));
    ASSERT_EQ(SRC_DRIVE, s.source);
    ASSERT_STR_EQ("", s.server);
    PASS();
}

TEST settings_save_then_load(void)
{
    clean_dir();
    ctw_settings in = { SRC_SERVER, "PC-NAME", ASP_FIT };
    ctw_settings out = { SRC_DRIVE, "", ASP_FIT };
    ASSERT_EQ(0, settings_save(DIR, &in));
    ASSERT(file_exists(DIR "/settings.toml"));
    ASSERT(!file_exists(DIR "/settings.toml.tmp"));
    settings_load(DIR, &out);
    ASSERT_EQ(SRC_SERVER, out.source);
    ASSERT_STR_EQ("PC-NAME", out.server);

    ctw_settings again = { SRC_DRIVE, "10.0.0.2", ASP_FILL };
    ASSERT_EQ(0, settings_save(DIR, &again));
    settings_load(DIR, &out);
    ASSERT_EQ(SRC_DRIVE, out.source);
    ASSERT_STR_EQ("10.0.0.2", out.server);
    ASSERT_EQ(ASP_FILL, out.picture);
    clean_dir();
    PASS();
}

TEST settings_syntax_error_gives_defaults(void)
{
    ctw_settings s = { SRC_SERVER, "old", ASP_FIT };
    ASSERT_EQ(-1, settings_parse("source = \"server", &s));
    ASSERT_EQ(SRC_DRIVE, s.source);
    ASSERT_STR_EQ("", s.server);
    PASS();
}

TEST settings_refuses_long_server(void)
{
    char text[300];
    char longest[128];
    memset(longest, 'a', 127);
    longest[127] = '\0';
    ctw_settings s = { SRC_DRIVE, "", ASP_FIT };
    snprintf(text, sizeof text, "source = \"server\"\nserver = \"%s\"\n", longest);
    ASSERT_EQ(0, settings_parse(text, &s));
    ASSERT_STR_EQ(longest, s.server);
    snprintf(text, sizeof text, "source = \"server\"\nserver = \"%sb\"\n", longest);
    ASSERT_EQ(-1, settings_parse(text, &s));
    ASSERT_EQ(SRC_DRIVE, s.source);
    ASSERT_STR_EQ("", s.server);
    PASS();
}

TEST settings_format_escapes_quotes(void)
{
    ctw_settings in = { SRC_SERVER, "a\"b\\c\td", ASP_FIT };
    ctw_settings out = { SRC_DRIVE, "", ASP_FIT };
    char buf[256];
    ASSERT(settings_format(&in, buf, sizeof buf) > 0);
    ASSERT_STR_EQ("source = \"server\"\nserver = \"a\\\"b\\\\c\\u0009d\"\npicture = \"fit\"\n", buf);
    ASSERT_EQ(0, settings_parse(buf, &out));
    ASSERT_STR_EQ(in.server, out.server);
    PASS();
}

TEST settings_format_too_small(void)
{
    ctw_settings in = { SRC_SERVER, "192.168.1.20:4747", ASP_FIT };
    char buf[20];
    ASSERT_EQ(0u, settings_format(&in, buf, sizeof buf));
    ASSERT_STR_EQ("", buf);
    PASS();
}

TEST settings_save_into_missing_folder_fails(void)
{
    ctw_settings in = { SRC_SERVER, "pc", ASP_FIT };
    ASSERT_EQ(-1, settings_save(DIR "/missing", &in));
    ASSERT(!file_exists(DIR "/missing/settings.toml"));
    ASSERT(!file_exists(DIR "/missing/settings.toml.tmp"));
    PASS();
}

TEST settings_equal_compares_source_and_address(void)
{
    ctw_settings a = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings same = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings other_source = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ctw_settings other_address = { SRC_SERVER, "192.168.1.21", ASP_FIT };
    ASSERT(settings_equal(&a, &same));
    ASSERT_FALSE(settings_equal(&a, &other_source));
    ASSERT_FALSE(settings_equal(&a, &other_address));
    PASS();
}

TEST settings_restarts_when_the_source_changes(void)
{
    ctw_settings drive = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ctw_settings server = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ASSERT(settings_restarts(&drive, &server));
    ASSERT(settings_restarts(&server, &drive));
    PASS();
}

TEST settings_restarts_when_the_server_address_changes(void)
{
    ctw_settings was = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings now = { SRC_SERVER, "http://192.168.1.20", ASP_FIT };
    ASSERT(settings_restarts(&was, &now));
    PASS();
}

TEST settings_keeps_the_drive_when_only_the_address_changes(void)
{
    ctw_settings was = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings now = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ASSERT_FALSE(settings_restarts(&was, &now));
    ASSERT_FALSE(settings_equal(&was, &now));
    PASS();
}

TEST settings_nothing_changed_keeps_the_source(void)
{
    ctw_settings was = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings now = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ASSERT_FALSE(settings_restarts(&was, &now));
    PASS();
}

TEST settings_picture_values(void)
{
    ctw_settings s = { SRC_DRIVE, "", ASP_FILL };
    ASSERT_EQ(0, settings_parse("picture = \"fit\"\n", &s));
    ASSERT_EQ(ASP_FIT, s.picture);
    ASSERT_EQ(0, settings_parse("picture = \"14:9\"\n", &s));
    ASSERT_EQ(ASP_14_9, s.picture);
    ASSERT_EQ(0, settings_parse("picture = \"fill\"\n", &s));
    ASSERT_EQ(ASP_FILL, s.picture);
    ASSERT_EQ(0, settings_parse("source = \"drive\"\n", &s));
    ASSERT_EQ(ASP_FIT, s.picture);
    PASS();
}

TEST settings_odd_picture_is_fit(void)
{
    const char *texts[] = {
        "source = \"server\"\nserver = \"pc\"\npicture = \"FILL\"\n",
        "source = \"server\"\nserver = \"pc\"\npicture = 3\n",
        "source = \"server\"\nserver = \"pc\"\npicture = \"fill the whole screen please\"\n",
    };
    for (size_t i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        ctw_settings s = { SRC_DRIVE, "", ASP_FILL };
        ASSERT_EQ(0, settings_parse(texts[i], &s));
        ASSERT_EQ(SRC_SERVER, s.source);
        ASSERT_STR_EQ("pc", s.server);
        ASSERT_EQ(ASP_FIT, s.picture);
    }
    PASS();
}

TEST settings_equal_compares_picture(void)
{
    ctw_settings fit = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings fill = { SRC_DRIVE, "", ASP_FILL };
    ASSERT_FALSE(settings_equal(&fit, &fill));
    PASS();
}

TEST settings_picture_never_restarts(void)
{
    ctw_settings fit = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings fill = { SRC_DRIVE, "", ASP_FILL };
    ctw_settings server_fit = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings server_wide = { SRC_SERVER, "192.168.1.20", ASP_14_9 };
    ASSERT_FALSE(settings_restarts(&fit, &fill));
    ASSERT_FALSE(settings_restarts(&server_fit, &server_wide));
    PASS();
}

SUITE(settings_suite)
{
    RUN_TEST(settings_round_trip);
    RUN_TEST(settings_defaults_when_missing);
    RUN_TEST(settings_unknown_source_is_drive);
    RUN_TEST(settings_save_then_load);
    RUN_TEST(settings_syntax_error_gives_defaults);
    RUN_TEST(settings_refuses_long_server);
    RUN_TEST(settings_format_escapes_quotes);
    RUN_TEST(settings_format_too_small);
    RUN_TEST(settings_save_into_missing_folder_fails);
    RUN_TEST(settings_equal_compares_source_and_address);
    RUN_TEST(settings_restarts_when_the_source_changes);
    RUN_TEST(settings_restarts_when_the_server_address_changes);
    RUN_TEST(settings_keeps_the_drive_when_only_the_address_changes);
    RUN_TEST(settings_nothing_changed_keeps_the_source);
    RUN_TEST(settings_picture_values);
    RUN_TEST(settings_odd_picture_is_fit);
    RUN_TEST(settings_equal_compares_picture);
    RUN_TEST(settings_picture_never_restarts);
}
