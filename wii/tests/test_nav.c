#include <string.h>
#include "greatest/greatest.h"
#include "nav.h"

static ctw_db *parse_channels(const char *toml)
{
    ctw_db *db = ctw_db_new();
    ctw_issues issues;
    memset(&issues, 0, sizeof issues);
    ctw_library_parse(db, "version = 1\n", &issues);
    ctw_issues_free(&issues);
    memset(&issues, 0, sizeof issues);
    ctw_config_parse(db, toml, &issues);
    ctw_issues_free(&issues);
    return db;
}

static const char *CH_123 =
    "version = 1\nseed = 1\n"
    "[[channel]]\nnumber = 1\nname = \"A\"\ncategory = \"X\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n"
    "[[channel]]\nnumber = 2\nname = \"B\"\ncategory = \"X\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n"
    "[[channel]]\nnumber = 3\nname = \"C\"\ncategory = \"X\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n";

static const char *CH_159 =
    "version = 1\nseed = 1\n"
    "[[channel]]\nnumber = 1\nname = \"A\"\ncategory = \"X\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n"
    "[[channel]]\nnumber = 5\nname = \"B\"\ncategory = \"X\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n"
    "[[channel]]\nnumber = 9\nname = \"C\"\ncategory = \"X\"\nads = true\nmode = \"looping\"\nloop = [{ type = \"ads\", count = 1 }]\n";

TEST nav_wraps_forward(void)
{
    ctw_db *db = parse_channels(CH_123);
    ASSERT_EQ(1, nav_step(db, 3, 1));
    ctw_db_free(db);
    PASS();
}

TEST nav_wraps_back(void)
{
    ctw_db *db = parse_channels(CH_123);
    ASSERT_EQ(3, nav_step(db, 1, -1));
    ctw_db_free(db);
    PASS();
}

TEST nav_gaps_in_numbers(void)
{
    ctw_db *db = parse_channels(CH_159);
    ASSERT_EQ(5, nav_step(db, 1, 1));
    ctw_db_free(db);
    PASS();
}

TEST nav_unknown_current(void)
{
    ctw_db *db = parse_channels(CH_159);
    ASSERT_EQ(5, nav_step(db, 4, 1));
    ASSERT_EQ(1, nav_step(db, 4, -1));
    ctw_db_free(db);
    PASS();
}

SUITE(nav_suite)
{
    RUN_TEST(nav_wraps_forward);
    RUN_TEST(nav_wraps_back);
    RUN_TEST(nav_gaps_in_numbers);
    RUN_TEST(nav_unknown_current);
}
