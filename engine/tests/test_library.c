#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"
#include "../src/internal.h"

#define FIXTURE_PATH "tests/fixtures/lib_basic.toml"

static char *read_fixture(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf) buf[n] = '\0';
    fclose(f);
    return buf;
}

#define FIXTURE_OK(t) \
    do { if (!(t)) FAILm("cannot read " FIXTURE_PATH); } while (0)

static const char *HEAD = "version = 1\n\n";

static const char *SHOW_HEAD =
    "[[show]]\nchannel = \"c\"\nfolder = \"f\"\nname = \"n\"\nicon = \"\"\n\n";

static int parse_text(const char *text, ctw_issues *is)
{
    ctw_db *db = ctw_db_new();
    int rc = ctw_library_parse(db, text, is);
    ctw_db_free(db);
    return rc;
}

static int parse_episode(const char *episode_body, ctw_issues *is)
{
    char text[1024];
    snprintf(text, sizeof text, "%s%s[[show.episode]]\n%s\n", HEAD, SHOW_HEAD, episode_body);
    return parse_text(text, is);
}

TEST library_basic_counts(void)
{
    char *text = read_fixture(FIXTURE_PATH);
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    FIXTURE_OK(text);
    ASSERT_EQ(0, ctw_library_parse(db, text, &is));
    ASSERT_EQ(0u, is.count);
    ASSERT_EQ(4, ctw_db_show_count(db));
    ASSERT_EQ(3, ctw_db_ad_count(db));
    ctw_issues_free(&is);
    ctw_db_free(db);
    free(text);
    PASS();
}

TEST library_episodes_sorted(void)
{
    char *text = read_fixture(FIXTURE_PATH);
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    const ctw_show *g;
    const ctw_show *m;
    FIXTURE_OK(text);
    ASSERT_EQ(0, ctw_library_parse(db, text, &is));
    g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ASSERT(g != NULL);
    ASSERT_EQ(6, g->episode_count);
    ASSERT_EQ(1, g->episodes[0].season);
    ASSERT_EQ(1, g->episodes[0].episode);
    ASSERT_EQ(2, g->episodes[5].season);
    ASSERT_EQ(2, g->episodes[5].episode);
    ASSERT_EQ(600000u, g->episodes[0].duration_ms);
    m = ctw_db_show(db, "mtv-00s", "Music Videos");
    ASSERT(m != NULL);
    ASSERT_EQ(6, m->episode_count);
    ASSERT_EQ(0, m->episodes[0].season);
    ASSERT_STR_EQ("", m->icon);
    ASSERT_STR_EQ("mtv-00s", m->channel_folder);
    ASSERT(ctw_db_show(db, "mtv-00s", "Gumball") == NULL);
    ASSERT(ctw_db_show(db, "Cartoon Network", "Missing") == NULL);
    ctw_issues_free(&is);
    ctw_db_free(db);
    free(text);
    PASS();
}

TEST library_accessors_and_ads(void)
{
    char *text = read_fixture(FIXTURE_PATH);
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    FIXTURE_OK(text);
    ASSERT_EQ(0, ctw_library_parse(db, text, &is));
    ASSERT_STR_EQ("Cartoon Network", ctw_db_show_at(db, 0)->channel_folder);
    ASSERT_STR_EQ("Ben 10", ctw_db_show_at(db, 2)->folder);
    ASSERT(ctw_db_show_at(db, 4) == NULL);
    ASSERT_STR_EQ("ads/ad-b.mpg", ctw_db_ad_at(db, 1)->path);
    ASSERT_STR_EQ("", ctw_db_ad_at(db, 1)->channel_folder);
    ASSERT_EQ(15000u, ctw_db_ad_at(db, 1)->duration_ms);
    ASSERT(ctw_db_ad_at(db, 3) == NULL);
    ctw_issues_free(&is);
    ctw_db_free(db);
    free(text);
    PASS();
}

TEST library_strings_survive_input_free(void)
{
    char *text = read_fixture(FIXTURE_PATH);
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    const ctw_show *g;
    FIXTURE_OK(text);
    ASSERT_EQ(0, ctw_library_parse(db, text, &is));
    memset(text, 'x', strlen(text));
    free(text);
    g = ctw_db_show(db, "Cartoon Network", "Gumball");
    ASSERT(g != NULL);
    ASSERT_STR_EQ("Cartoon Network/Gumball/Season 1/S01E01 - Gumball 1.mpg", g->episodes[0].path);
    ASSERT_STR_EQ("The Amazing World of Gumball", g->name);
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST library_unsorted_input_sorted(void)
{
    const char *text =
        "version = 1\n[[show]]\nchannel = \"c\"\nfolder = \"f\"\nname = \"n\"\nicon = \"\"\n"
        "[[show.episode]]\nseason = 2\nepisode = 1\npath = \"p21\"\nduration_ms = 3\n"
        "[[show.episode]]\nseason = 1\nepisode = 2\npath = \"p12\"\nduration_ms = 2\n"
        "[[show.episode]]\nseason = 1\nepisode = 1\npath = \"p11\"\nduration_ms = 1\n";
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    const ctw_show *s;
    ASSERT_EQ(0, ctw_library_parse(db, text, &is));
    s = ctw_db_show(db, "c", "f");
    ASSERT_EQ(3, s->episode_count);
    ASSERT_STR_EQ("p11", s->episodes[0].path);
    ASSERT_STR_EQ("p12", s->episodes[1].path);
    ASSERT_STR_EQ("p21", s->episodes[2].path);
    ASSERT_STR_EQ("", s->episodes[0].title);
    ASSERT_STR_EQ("", s->episodes[0].description);
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST library_missing_duration(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\npath = \"p\"", &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT_EQ(CTW_LEVEL_ERROR, is.items[0].level);
    ASSERT_STR_EQ("library", is.items[0].file);
    ASSERT_EQ(-1, is.items[0].channel);
    ASSERT_EQ(-1, is.items[0].slot);
    ASSERT_EQ(-1, is.items[0].item);
    ctw_issues_free(&is);
    PASS();
}

TEST library_rejects_zero_duration(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\npath = \"p\"\nduration_ms = 0", &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    PASS();
}

TEST library_duration_range(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\npath = \"p\"\nduration_ms = -5", &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\npath = \"p\"\nduration_ms = 4294967296", &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(0, parse_episode("season = 1\nepisode = 1\npath = \"p\"\nduration_ms = 4294967295", &is));
    ASSERT_EQ(0u, is.count);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(0, parse_episode("season = 1\nepisode = 1\npath = \"p\"\nduration_ms = 1", &is));
    ctw_issues_free(&is);
    PASS();
}

TEST library_wrong_type_is_parse(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("season = \"1\"\nepisode = 1\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\npath = 5\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    PASS();
}

TEST library_season_episode_range(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("season = 65536\nepisode = 1\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = -1\nepisode = 1\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 0\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 65536\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(0, parse_episode("season = 0\nepisode = 65535\npath = \"p\"\nduration_ms = 1", &is));
    ctw_issues_free(&is);
    PASS();
}

TEST library_missing_required_fields(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("episode = 1\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = 1\npath = \"p\"\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\nduration_ms = 1", &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    PASS();
}

TEST library_collects_all_errors(void)
{
    const char *text =
        "version = 1\n"
        "[[ad]]\npath = \"a\"\nchannel = \"\"\nduration_ms = 0\n"
        "[[ad]]\npath = \"b\"\nchannel = \"\"\n"
        "[[show]]\nchannel = \"c\"\nfolder = \"f\"\nname = \"n\"\nicon = \"\"\n"
        "[[show.episode]]\nseason = 1\nepisode = 1\npath = \"p\"\nduration_ms = 0\n"
        "[[show.episode]]\nseason = 1\nepisode = 2\npath = \"q\"\nduration_ms = 5\n";
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    ASSERT_EQ(-1, ctw_library_parse(db, text, &is));
    ASSERT_EQ(3u, is.count);
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ASSERT_EQ(CTW_E_PARSE, is.items[1].code);
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[2].code);
    ASSERT(ctw_issues_has_errors(&is));
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST library_wrong_version(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_text("version = 2\n", &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_VERSION, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_text("version = \"1\"\n", &is));
    ASSERT_EQ(CTW_E_VERSION, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_text("seed = 1\n", &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(0, parse_text("version = 1\n", &is));
    ASSERT_EQ(0u, is.count);
    ctw_issues_free(&is);
    PASS();
}

TEST library_syntax_error_line(void)
{
    const char *text =
        "version = 1\n"
        "\n"
        "[[show]]\n"
        "channel = \"c\"\n"
        "folder = \"f\"\n"
        "name = \"n\"\n"
        "icon = = \"\"\n";
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_text(text, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT_EQ(7, is.items[0].line);
    ASSERT_STR_EQ("library", is.items[0].file);
    ctw_issues_free(&is);
    PASS();
}

TEST library_field_errors_have_no_line(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_episode("season = 1\nepisode = 1\npath = \"p\"\nduration_ms = 0", &is));
    ASSERT_EQ(0, is.items[0].line);
    ASSERT(strstr(is.items[0].message, "duration_ms") != NULL);
    ctw_issues_free(&is);
    PASS();
}

TEST library_ad_not_table(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_text("version = 1\nad = [1, 2]\n", &is));
    ASSERT(is.count >= 1);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT(strstr(is.items[0].message, "ad #1") != NULL);
    ctw_issues_free(&is);
    PASS();
}

TEST library_show_not_table(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_text("version = 1\nshow = [1]\n", &is));
    ASSERT(is.count >= 1);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT(strstr(is.items[0].message, "show #1") != NULL);
    ctw_issues_free(&is);
    PASS();
}

TEST library_episode_not_table(void)
{
    char text[512];
    ctw_issues is = {0};
    snprintf(text, sizeof text, "%s[[show]]\nchannel = \"c\"\nfolder = \"f\"\nname = \"n\"\n"
             "icon = \"\"\nepisode = [1]\n", HEAD);
    ASSERT_EQ(-1, parse_text(text, &is));
    ASSERT(is.count >= 1);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT(strstr(is.items[0].message, "episode #1") != NULL);
    ctw_issues_free(&is);
    PASS();
}

TEST library_wrong_type_top_level(void)
{
    ctw_issues is = {0};
    ASSERT_EQ(-1, parse_text("version = 1\nad = \"x\"\n", &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    memset(&is, 0, sizeof is);
    ASSERT_EQ(-1, parse_text("version = 1\nshow = \"x\"\n", &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ctw_issues_free(&is);
    PASS();
}

TEST db_free_null_is_safe(void)
{
    ctw_db_free(NULL);
    PASS();
}

TEST db_empty_accessors(void)
{
    ctw_db *db = ctw_db_new();
    ASSERT(db != NULL);
    ASSERT_EQ(0, ctw_db_show_count(db));
    ASSERT_EQ(0, ctw_db_ad_count(db));
    ASSERT(ctw_db_show_at(db, 0) == NULL);
    ASSERT(ctw_db_show(db, "a", "b") == NULL);
    ctw_db_free(db);
    PASS();
}

TEST issues_helpers(void)
{
    ctw_issues is = {0};
    int i;
    ASSERT(!ctw_issues_has_errors(&is));
    ctw_issues_add(&is, CTW_LEVEL_WARNING, CTW_W_CUT, 3, 1, 2, 9, "config", "warn %d", 5);
    ASSERT(!ctw_issues_has_errors(&is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(3, is.items[0].channel);
    ASSERT_EQ(1, is.items[0].slot);
    ASSERT_EQ(2, is.items[0].item);
    ASSERT_EQ(9, is.items[0].line);
    ASSERT_STR_EQ("config", is.items[0].file);
    ASSERT_STR_EQ("warn 5", is.items[0].message);
    ctw_issues_add(&is, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, "a-very-long-file-name-here", "%s", "x");
    ASSERT(ctw_issues_has_errors(&is));
    ASSERT_EQ(15, strlen(is.items[1].file));
    for (i = 0; i < 100; i++)
        ctw_issues_add(&is, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, "library", "n%d", i);
    ASSERT_EQ(102u, is.count);
    ASSERT_STR_EQ("n99", is.items[101].message);
    ctw_issues_free(&is);
    ASSERT_EQ(0u, is.count);
    ASSERT(is.items == NULL);
    ctw_issues_free(&is);
    ctw_issues_free(NULL);
    ASSERT(!ctw_issues_has_errors(NULL));
    PASS();
}

TEST issues_message_truncates(void)
{
    ctw_issues is = {0};
    char big[400];
    memset(big, 'z', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    ctw_issues_add(&is, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, "library", "%s", big);
    ASSERT_EQ(199, strlen(is.items[0].message));
    ctw_issues_free(&is);
    PASS();
}

TEST arena_basics(void)
{
    ctw_arena a;
    char *s;
    char *t;
    void *p;
    void *q;
    char *huge;
    ctw_arena_init(&a);
    s = ctw_arena_strdup(&a, "hello");
    ASSERT_STR_EQ("hello", s);
    p = ctw_arena_alloc(&a, 3);
    q = ctw_arena_alloc(&a, 8);
    ASSERT(p != NULL && q != NULL);
    ASSERT_EQ(0u, (unsigned)((size_t)q % sizeof(void *)));
    huge = ctw_arena_alloc(&a, 100000);
    ASSERT(huge != NULL);
    memset(huge, 1, 100000);
    t = ctw_arena_strdup(&a, "");
    ASSERT_STR_EQ("", t);
    ASSERT_STR_EQ("hello", s);
    ASSERT(ctw_arena_strdup(&a, NULL) == NULL);
    ctw_arena_free(&a);
    ctw_arena_free(&a);
    PASS();
}

SUITE(library_suite)
{
    RUN_TEST(library_basic_counts);
    RUN_TEST(library_episodes_sorted);
    RUN_TEST(library_accessors_and_ads);
    RUN_TEST(library_strings_survive_input_free);
    RUN_TEST(library_unsorted_input_sorted);
    RUN_TEST(library_missing_duration);
    RUN_TEST(library_rejects_zero_duration);
    RUN_TEST(library_duration_range);
    RUN_TEST(library_wrong_type_is_parse);
    RUN_TEST(library_season_episode_range);
    RUN_TEST(library_missing_required_fields);
    RUN_TEST(library_collects_all_errors);
    RUN_TEST(library_wrong_version);
    RUN_TEST(library_syntax_error_line);
    RUN_TEST(library_field_errors_have_no_line);
    RUN_TEST(library_ad_not_table);
    RUN_TEST(library_show_not_table);
    RUN_TEST(library_episode_not_table);
    RUN_TEST(library_wrong_type_top_level);
    RUN_TEST(db_free_null_is_safe);
    RUN_TEST(db_empty_accessors);
    RUN_TEST(issues_helpers);
    RUN_TEST(issues_message_truncates);
    RUN_TEST(arena_basics);
}
