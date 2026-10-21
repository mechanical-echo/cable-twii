#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"

#define LIB_PATH "tests/fixtures/lib_basic.toml"

#define HEAD "version = 1\nseed = 7\n\n"

#define CN_HEAD \
    "[[channel]]\nnumber = 1\nname = \"Cartoon Network\"\ncategory = \"Kids\"\n" \
    "ads = true\nmode = \"timed\"\n"

#define CN_ONE_SLOT(items) \
    HEAD CN_HEAD "[[channel.slot]]\nat = \"07:00\"\nitems = [ " items " ]\n"

#define MTV_HEAD \
    "[[channel]]\nnumber = 2\nname = \"MTV\"\ncategory = \"Music\"\nfolder = \"mtv-00s\"\n" \
    "ads = false\nmode = \"looping\"\n"

#define MTV_LOOP(items) HEAD MTV_HEAD "loop = [ " items " ]\n"

static char *read_file(const char *path)
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

static ctw_db *load_library(ctw_issues *is)
{
    char *text = read_file(LIB_PATH);
    ctw_db *db = ctw_db_new();
    int rc = text ? ctw_library_parse(db, text, is) : -1;
    free(text);
    if (rc) {
        ctw_db_free(db);
        return NULL;
    }
    return db;
}

static int run_config(const char *cfg, ctw_db **out, ctw_issues *is)
{
    ctw_issues libis = {0};
    ctw_db *db = load_library(&libis);
    int rc;
    ctw_issues_free(&libis);
    if (!db) return -99;
    rc = ctw_config_parse(db, cfg, is);
    *out = db;
    return rc;
}

static int has_code(const ctw_issues *is, ctw_code code)
{
    size_t i;
    for (i = 0; i < is->count; i++)
        if (is->items[i].code == code) return 1;
    return 0;
}

#define SETUP() \
    ctw_db *db = NULL; \
    ctw_issues is = {0}

#define TEARDOWN() \
    do { ctw_issues_free(&is); ctw_db_free(db); } while (0)

#define EXPECT_ERROR(cfg, code) \
    do { \
        SETUP(); \
        ASSERT_EQ(-1, run_config((cfg), &db, &is)); \
        ASSERT(has_code(&is, (code))); \
        ASSERT_STR_EQ("config", is.items[0].file); \
        TEARDOWN(); \
        PASS(); \
    } while (0)

TEST config_defaults(void)
{
    SETUP();
    const ctw_channel *c;
    ASSERT_EQ(0, run_config(CN_ONE_SLOT("{ type = \"ads\" }"), &db, &is));
    ASSERT_EQ(0u, is.count);
    ASSERT_EQ(7u, ctw_db_seed(db));
    ASSERT_EQ(1u, ctw_db_channel_count(db));
    c = ctw_db_channel_at(db, 0);
    ASSERT_STR_EQ("Cartoon Network", c->folder);
    ASSERT_STR_EQ("Cartoon Network/channel.png", c->icon);
    ASSERT_STR_EQ("Cartoon Network", c->name);
    ASSERT_STR_EQ("Kids", c->category);
    ASSERT(c->ads);
    ASSERT_EQ(CTW_MODE_TIMED, c->mode);
    TEARDOWN();
    PASS();
}

TEST config_icon_relative_to_folder(void)
{
    SETUP();
    ASSERT_EQ(0, run_config(HEAD
        "[[channel]]\nnumber = 3\nname = \"X\"\ncategory = \"c\"\nfolder = \"mtv-00s\"\n"
        "icon = \"logo.png\"\nads = false\nmode = \"looping\"\nloop = [ { type = \"ads\" } ]\n",
        &db, &is));
    ASSERT_STR_EQ("mtv-00s", ctw_db_channel(db, 3)->folder);
    ASSERT_STR_EQ("mtv-00s/logo.png", ctw_db_channel(db, 3)->icon);
    TEARDOWN();
    PASS();
}

TEST config_empty_config(void)
{
    SETUP();
    ASSERT_EQ(0, run_config(HEAD, &db, &is));
    ASSERT_EQ(0u, ctw_db_channel_count(db));
    ASSERT(ctw_db_channel_at(db, 0) == NULL);
    ASSERT(ctw_db_channel(db, 1) == NULL);
    TEARDOWN();
    PASS();
}

TEST config_channels_sorted(void)
{
    SETUP();
    ASSERT_EQ(0, run_config(HEAD
        "[[channel]]\nnumber = 2\nname = \"B\"\ncategory = \"c\"\nads = false\n"
        "mode = \"looping\"\nloop = [ { type = \"ads\" } ]\n"
        "[[channel]]\nnumber = 1\nname = \"A\"\ncategory = \"c\"\nads = false\n"
        "mode = \"looping\"\nloop = [ { type = \"ads\" } ]\n",
        &db, &is));
    ASSERT_EQ(2u, ctw_db_channel_count(db));
    ASSERT_EQ(1, ctw_db_channel_at(db, 0)->number);
    ASSERT_EQ(2, ctw_db_channel_at(db, 1)->number);
    ASSERT_STR_EQ("B", ctw_db_channel(db, 2)->name);
    ASSERT(ctw_db_channel(db, 3) == NULL);
    TEARDOWN();
    PASS();
}

TEST config_slots_sorted(void)
{
    SETUP();
    const ctw_channel *c;
    ASSERT_EQ(0, run_config(HEAD CN_HEAD
        "[[channel.slot]]\nat = \"19:00\"\nitems = [ { type = \"ads\", count = 2 } ]\n"
        "[[channel.slot]]\nat = \"07:00\"\nitems = [ { type = \"ads\", count = 3 } ]\n",
        &db, &is));
    c = ctw_db_channel(db, 1);
    ASSERT_EQ(2u, c->slot_count);
    ASSERT_EQ(25200000u, c->slots[0].at_ms);
    ASSERT_EQ(3, c->slots[0].items[0].count);
    ASSERT_EQ(68400000u, c->slots[1].at_ms);
    ASSERT_EQ(2, c->slots[1].items[0].count);
    TEARDOWN();
    PASS();
}

TEST config_items_resolved(void)
{
    SETUP();
    const ctw_item *it;
    ASSERT_EQ(0, run_config(CN_ONE_SLOT(
        "{ type = \"random\", show = \"Adventure Time\", season = 5, count = 5 }"), &db, &is));
    it = &ctw_db_channel(db, 1)->slots[0].items[0];
    ASSERT_EQ(CTW_ITEM_RANDOM, it->type);
    ASSERT(it->show == ctw_db_show(db, "Cartoon Network", "Adventure Time"));
    ASSERT_EQ(5, it->season);
    ASSERT_EQ(5, it->count);
    TEARDOWN();
    PASS();
}

TEST config_item_defaults(void)
{
    SETUP();
    const ctw_slot *s;
    ASSERT_EQ(0, run_config(CN_ONE_SLOT(
        "{ type = \"random-run\", show = \"Gumball\" }, { type = \"fill\", show = \"Gumball\" }, "
        "{ type = \"ads\" }"), &db, &is));
    s = &ctw_db_channel(db, 1)->slots[0];
    ASSERT_EQ(3u, s->item_count);
    ASSERT_EQ(CTW_ITEM_RANDOM_RUN, s->items[0].type);
    ASSERT_EQ(1, s->items[0].count);
    ASSERT_EQ(0, s->items[0].season);
    ASSERT_EQ(CTW_ITEM_FILL, s->items[1].type);
    ASSERT_EQ(0, s->items[1].ads_between);
    ASSERT_EQ(CTW_ITEM_ADS, s->items[2].type);
    ASSERT_EQ(1, s->items[2].count);
    TEARDOWN();
    PASS();
}

TEST config_episode_item(void)
{
    SETUP();
    const ctw_item *it;
    ASSERT_EQ(0, run_config(CN_ONE_SLOT(
        "{ type = \"episode\", show = \"Ben 10\", season = 1, episode = 5 }"), &db, &is));
    it = &ctw_db_channel(db, 1)->slots[0].items[0];
    ASSERT_EQ(CTW_ITEM_EPISODE, it->type);
    ASSERT_EQ(1, it->season);
    ASSERT_EQ(5, it->episode);
    ASSERT(it->show == ctw_db_show(db, "Cartoon Network", "Ben 10"));
    TEARDOWN();
    PASS();
}

TEST config_episode_seasonless(void)
{
    SETUP();
    const ctw_item *it;
    ASSERT_EQ(0, run_config(MTV_LOOP(
        "{ type = \"episode\", show = \"Music Videos\", season = 0, episode = 3 }"), &db, &is));
    it = &ctw_db_channel(db, 2)->loop[0];
    ASSERT_EQ(0, it->season);
    ASSERT_EQ(3, it->episode);
    ASSERT(it->show == ctw_db_show(db, "mtv-00s", "Music Videos"));
    TEARDOWN();
    PASS();
}

TEST config_episode_season_must_match(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"episode\", show = \"Gumball\", season = 0, episode = 1 }"), &db, &is));
    ASSERT(has_code(&is, CTW_E_MISSING_EPISODE));
    TEARDOWN();
    PASS();
}

TEST config_random_season_zero_is_all(void)
{
    SETUP();
    ASSERT_EQ(0, run_config(MTV_LOOP(
        "{ type = \"random\", show = \"Music Videos\", season = 0, count = 2 }"), &db, &is));
    TEARDOWN();
    PASS();
}

TEST config_from_other_channel(void)
{
    SETUP();
    const ctw_item *it;
    ASSERT_EQ(0, run_config(MTV_LOOP(
        "{ type = \"random\", show = \"Gumball\", from = \"Cartoon Network\" }"), &db, &is));
    it = &ctw_db_channel(db, 2)->loop[0];
    ASSERT(it->show == ctw_db_show(db, "Cartoon Network", "Gumball"));
    TEARDOWN();
    PASS();
}

TEST config_specific_ad(void)
{
    SETUP();
    const ctw_item *it;
    ASSERT_EQ(0, run_config(CN_ONE_SLOT("{ type = \"ad\", path = \"ads/ad-b.mpg\" }"), &db, &is));
    it = &ctw_db_channel(db, 1)->slots[0].items[0];
    ASSERT_EQ(CTW_ITEM_AD, it->type);
    ASSERT_EQ(15000u, it->ad->duration_ms);
    TEARDOWN();
    PASS();
}

TEST config_dup_channel(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD
        "[[channel]]\nnumber = 1\nname = \"A\"\ncategory = \"c\"\nads = false\n"
        "mode = \"looping\"\nloop = [ { type = \"ads\" } ]\n"
        "[[channel]]\nnumber = 1\nname = \"B\"\ncategory = \"c\"\nads = false\n"
        "mode = \"looping\"\nloop = [ { type = \"ads\" } ]\n", &db, &is));
    ASSERT(has_code(&is, CTW_E_DUP_CHANNEL));
    ASSERT_EQ(1, is.items[0].channel);
    TEARDOWN();
    PASS();
}

TEST config_dup_slot(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD CN_HEAD
        "[[channel.slot]]\nat = \"07:00\"\nitems = [ { type = \"ads\" } ]\n"
        "[[channel.slot]]\nat = \"07:00\"\nitems = [ { type = \"ads\" } ]\n", &db, &is));
    ASSERT(has_code(&is, CTW_E_DUP_SLOT));
    ASSERT_EQ(1, is.items[0].channel);
    ASSERT_EQ(1, is.items[0].slot);
    TEARDOWN();
    PASS();
}

TEST config_unknown_show(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT("{ type = \"random\", show = \"Nope\" }"), &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_UNKNOWN_SHOW, is.items[0].code);
    ASSERT_EQ(CTW_LEVEL_ERROR, is.items[0].level);
    ASSERT_EQ(1, is.items[0].channel);
    ASSERT_EQ(0, is.items[0].slot);
    ASSERT_EQ(0, is.items[0].item);
    ASSERT_EQ(0, is.items[0].line);
    ASSERT_STR_EQ("config", is.items[0].file);
    TEARDOWN();
    PASS();
}

TEST config_unknown_show_in_loop_indexes(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(MTV_LOOP(
        "{ type = \"ads\" }, { type = \"random\", show = \"Nope\" }"), &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_UNKNOWN_SHOW, is.items[0].code);
    ASSERT_EQ(2, is.items[0].channel);
    ASSERT_EQ(-1, is.items[0].slot);
    ASSERT_EQ(1, is.items[0].item);
    TEARDOWN();
    PASS();
}

TEST config_unknown_show_from(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"random\", show = \"Gumball\", from = \"mtv-00s\" }"), &db, &is));
    ASSERT(has_code(&is, CTW_E_UNKNOWN_SHOW));
    TEARDOWN();
    PASS();
}

TEST config_missing_episode(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"episode\", show = \"Ben 10\", season = 1, episode = 6 }"), &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_MISSING_EPISODE, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_unknown_ad(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT("{ type = \"ad\", path = \"ads/none.mpg\" }"), &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_UNKNOWN_AD, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_fill_in_loop(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(MTV_LOOP("{ type = \"fill\", show = \"Music Videos\" }"), &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_FILL_IN_LOOP, is.items[0].code);
    ASSERT_EQ(-1, is.items[0].slot);
    ASSERT_EQ(0, is.items[0].item);
    TEARDOWN();
    PASS();
}

TEST config_timed_without_slots(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD CN_HEAD, &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_NO_SLOTS, is.items[0].code);
    ASSERT_EQ(1, is.items[0].channel);
    TEARDOWN();
    PASS();
}

TEST config_bad_time(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD CN_HEAD
        "[[channel.slot]]\nat = \"25:00\"\nitems = [ { type = \"ads\" } ]\n", &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_BAD_TIME, is.items[0].code);
    ASSERT_EQ(0, is.items[0].slot);
    TEARDOWN();
    PASS();
}

TEST config_bad_type(void)
{
    EXPECT_ERROR(CN_ONE_SLOT("{ type = \"movie\" }"), CTW_E_BAD_VALUE);
}

TEST config_bad_mode(void)
{
    EXPECT_ERROR(HEAD
        "[[channel]]\nnumber = 1\nname = \"A\"\ncategory = \"c\"\nads = true\nmode = \"daily\"\n",
        CTW_E_BAD_VALUE);
}

TEST config_empty_pool_error(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"random\", show = \"Gumball\", season = 9 }"), &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_EMPTY_POOL, is.items[0].code);
    ASSERT_EQ(CTW_LEVEL_ERROR, is.items[0].level);
    TEARDOWN();
    PASS();
}

TEST config_empty_pool_run_and_fill(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"random-run\", show = \"Gumball\", season = 9 }, "
        "{ type = \"fill\", show = \"Gumball\", season = 9 }"), &db, &is));
    ASSERT_EQ(2u, is.count);
    ASSERT_EQ(CTW_E_EMPTY_POOL, is.items[0].code);
    ASSERT_EQ(0, is.items[0].item);
    ASSERT_EQ(CTW_E_EMPTY_POOL, is.items[1].code);
    ASSERT_EQ(1, is.items[1].item);
    TEARDOWN();
    PASS();
}

TEST config_loop_on_timed(void)
{
    EXPECT_ERROR(HEAD CN_HEAD "loop = [ { type = \"ads\" } ]\n"
                 "[[channel.slot]]\nat = \"07:00\"\nitems = [ { type = \"ads\" } ]\n",
                 CTW_E_BAD_VALUE);
}

TEST config_slot_on_looping(void)
{
    EXPECT_ERROR(MTV_LOOP("{ type = \"ads\" }")
                 "[[channel.slot]]\nat = \"07:00\"\nitems = [ { type = \"ads\" } ]\n",
                 CTW_E_BAD_VALUE);
}

TEST config_loop_missing(void)
{
    EXPECT_ERROR(HEAD MTV_HEAD, CTW_E_BAD_VALUE);
}

TEST config_loop_empty(void)
{
    EXPECT_ERROR(MTV_LOOP(""), CTW_E_BAD_VALUE);
}

TEST config_number_range(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD
        "[[channel]]\nnumber = 0\nname = \"A\"\ncategory = \"c\"\nads = false\n"
        "mode = \"looping\"\nloop = [ { type = \"ads\" } ]\n"
        "[[channel]]\nnumber = 1000\nname = \"B\"\ncategory = \"c\"\nads = false\n"
        "mode = \"looping\"\nloop = [ { type = \"ads\" } ]\n", &db, &is));
    ASSERT_EQ(2u, is.count);
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[1].code);
    TEARDOWN();
    PASS();
}

TEST config_item_ranges(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"ads\", count = 0 }, { type = \"ads\", count = 51 }, "
        "{ type = \"fill\", show = \"Gumball\", ads_between = 6 }, "
        "{ type = \"random\", show = \"Gumball\", season = 65536 }, "
        "{ type = \"episode\", show = \"Gumball\", season = 1, episode = 0 }, "
        "{ type = \"episode\", show = \"Gumball\", season = 1, episode = 65536 }"), &db, &is));
    ASSERT_EQ(6u, is.count);
    ASSERT(!has_code(&is, CTW_E_PARSE));
    ASSERT_EQ(5, is.items[5].item);
    TEARDOWN();
    PASS();
}

TEST config_item_range_edges(void)
{
    SETUP();
    ASSERT_EQ(0, run_config(CN_ONE_SLOT(
        "{ type = \"ads\", count = 50 }, "
        "{ type = \"random\", show = \"Gumball\", count = 1, season = 2 }, "
        "{ type = \"fill\", show = \"Gumball\", ads_between = 5 }"), &db, &is));
    TEARDOWN();
    PASS();
}

TEST config_seed_range(void)
{
    SETUP();
    ASSERT_EQ(0, run_config("version = 1\nseed = 4294967295\n", &db, &is));
    ASSERT_EQ(4294967295u, ctw_db_seed(db));
    ctw_issues_free(&is);
    ctw_db_free(db);
    db = NULL;
    ASSERT_EQ(-1, run_config("version = 1\nseed = 4294967296\n", &db, &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    ctw_db_free(db);
    db = NULL;
    ASSERT_EQ(-1, run_config("version = 1\nseed = -1\n", &db, &is));
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[0].code);
    ctw_issues_free(&is);
    ctw_db_free(db);
    db = NULL;
    ASSERT_EQ(-1, run_config("version = 1\n", &db, &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_version(void)
{
    EXPECT_ERROR("version = 2\nseed = 1\n", CTW_E_VERSION);
}

TEST config_syntax_error_line(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config("version = 1\nseed = \n= = =\n", &db, &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT(is.items[0].line > 0);
    TEARDOWN();
    PASS();
}

TEST config_missing_fields(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD "[[channel]]\n", &db, &is));
    ASSERT(has_code(&is, CTW_E_PARSE));
    ASSERT(is.count >= 5);
    TEARDOWN();
    PASS();
}

TEST config_wrong_typed_keys(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD
        "[[channel]]\nnumber = \"1\"\nname = 5\ncategory = true\nads = \"yes\"\nmode = 3\n",
        &db, &is));
    ASSERT_EQ(5u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    ASSERT_EQ(CTW_E_PARSE, is.items[1].code);
    ASSERT_EQ(CTW_E_PARSE, is.items[2].code);
    ASSERT_EQ(CTW_E_PARSE, is.items[3].code);
    ASSERT_EQ(CTW_E_PARSE, is.items[4].code);
    TEARDOWN();
    PASS();
}

TEST config_wrong_typed_item_keys(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = 5 }, { type = \"ads\", count = \"2\" }, { type = \"ad\", path = 3 }, "
        "{ type = \"random\", show = 1 }"), &db, &is));
    ASSERT_EQ(4u, is.count);
    ASSERT(!has_code(&is, CTW_E_BAD_VALUE));
    TEARDOWN();
    PASS();
}

TEST config_collects_all_errors(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT(
        "{ type = \"random\", show = \"Nope\" }, { type = \"ad\", path = \"x\" }, "
        "{ type = \"movie\" }, { type = \"random\", show = \"Gumball\", season = 9 }"), &db, &is));
    ASSERT_EQ(4u, is.count);
    ASSERT_EQ(CTW_E_UNKNOWN_SHOW, is.items[0].code);
    ASSERT_EQ(CTW_E_UNKNOWN_AD, is.items[1].code);
    ASSERT_EQ(CTW_E_BAD_VALUE, is.items[2].code);
    ASSERT_EQ(CTW_E_EMPTY_POOL, is.items[3].code);
    ASSERT_EQ(3, is.items[3].item);
    TEARDOWN();
    PASS();
}

TEST config_channel_not_table(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD "channel = [1, 2]\n", &db, &is));
    ASSERT_EQ(2u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_channel_wrong_top_level(void)
{
    EXPECT_ERROR(HEAD "channel = 5\n", CTW_E_PARSE);
}

TEST config_slot_not_table(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD CN_HEAD "slot = [1]\n", &db, &is));
    ASSERT(has_code(&is, CTW_E_PARSE));
    TEARDOWN();
    PASS();
}

TEST config_slot_wrong_type(void)
{
    EXPECT_ERROR(HEAD CN_HEAD "slot = 4\n", CTW_E_PARSE);
}

TEST config_items_not_array(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(HEAD CN_HEAD "[[channel.slot]]\nat = \"07:00\"\nitems = 5\n",
                             &db, &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_items_missing(void)
{
    EXPECT_ERROR(HEAD CN_HEAD "[[channel.slot]]\nat = \"07:00\"\n", CTW_E_PARSE);
}

TEST config_item_not_table(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(CN_ONE_SLOT("1, 2"), &db, &is));
    ASSERT_EQ(2u, is.count);
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_loop_not_array(void)
{
    EXPECT_ERROR(HEAD MTV_HEAD "loop = 5\n", CTW_E_PARSE);
}

TEST config_loop_item_not_table(void)
{
    SETUP();
    ASSERT_EQ(-1, run_config(MTV_LOOP("1"), &db, &is));
    ASSERT_EQ(CTW_E_PARSE, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_strings_survive_input_free(void)
{
    SETUP();
    char *text = strdup(CN_ONE_SLOT("{ type = \"ad\", path = \"ads/ad-a.mpg\" }"));
    ctw_issues libis = {0};
    db = load_library(&libis);
    ctw_issues_free(&libis);
    ASSERT(db != NULL);
    ASSERT_EQ(0, ctw_config_parse(db, text, &is));
    memset(text, 'x', strlen(text));
    free(text);
    ASSERT_STR_EQ("Cartoon Network", ctw_db_channel(db, 1)->name);
    ASSERT_STR_EQ("Cartoon Network/channel.png", ctw_db_channel(db, 1)->icon);
    TEARDOWN();
    PASS();
}

TEST config_requires_library(void)
{
    SETUP();
    db = ctw_db_new();
    ASSERT_EQ(-1, ctw_config_parse(db, CN_ONE_SLOT("{ type = \"random\", show = \"Gumball\" }"),
                                   &is));
    ASSERT_EQ(1u, is.count);
    ASSERT_EQ(CTW_E_UNKNOWN_SHOW, is.items[0].code);
    TEARDOWN();
    PASS();
}

TEST config_crlf_and_bom(void)
{
    static const char clean[] =
        "version = 1\nseed = 7\n\n"
        "[[channel]]\nnumber = 1\nname = \"Cartoon Network\"\ncategory = \"Kids\"\n"
        "ads = true\nmode = \"timed\"\n"
        "[[channel.slot]]\nat = \"19:00\"\n"
        "items = [ { type = \"fill\", show = \"Adventure Time\", ads_between = 2 } ]\n"
        "[[channel.slot]]\nat = \"07:00\"\n"
        "items = [ { type = \"random\", show = \"Gumball\", count = 2 }, { type = \"ads\" } ]\n";
    char dirty[sizeof clean * 2 + 8];
    const char *p;
    size_t n = 0;
    ctw_db *a = NULL;
    ctw_db *b = NULL;
    ctw_issues ia = {0};
    ctw_issues ib = {0};
    const ctw_channel *ca;
    const ctw_channel *cb;
    dirty[n++] = '\xEF';
    dirty[n++] = '\xBB';
    dirty[n++] = '\xBF';
    for (p = clean; *p; p++) {
        if (*p == '\n') dirty[n++] = '\r';
        dirty[n++] = *p;
    }
    dirty[n] = '\0';
    ASSERT_EQ(0, run_config(clean, &a, &ia));
    ASSERT_EQ(0, run_config(dirty, &b, &ib));
    ASSERT_EQ(0u, ia.count);
    ASSERT_EQ(0u, ib.count);
    ca = ctw_db_channel(a, 1);
    cb = ctw_db_channel(b, 1);
    ASSERT_EQ(ctw_db_seed(a), ctw_db_seed(b));
    ASSERT_STR_EQ(ca->name, cb->name);
    ASSERT_STR_EQ(ca->category, cb->category);
    ASSERT_STR_EQ(ca->icon, cb->icon);
    ASSERT_EQ(ca->slot_count, cb->slot_count);
    ASSERT_EQ(ca->slots[0].at_ms, cb->slots[0].at_ms);
    ASSERT_EQ(ca->slots[1].at_ms, cb->slots[1].at_ms);
    ASSERT_EQ(ca->slots[0].item_count, cb->slots[0].item_count);
    ASSERT_EQ(ca->slots[0].items[0].count, cb->slots[0].items[0].count);
    ASSERT_EQ(ca->slots[1].items[0].ads_between, cb->slots[1].items[0].ads_between);
    ASSERT_STR_EQ(cb->slots[0].items[0].show->folder, "Gumball");
    ctw_issues_free(&ia);
    ctw_issues_free(&ib);
    ctw_db_free(a);
    ctw_db_free(b);
    PASS();
}

TEST config_unicode_show_name(void)
{
    static const char lib[] =
        "version = 1\n[[show]]\nchannel = \"Cartoon Network\"\nfolder = \"Pok\xC3\xA9mon\"\n"
        "name = \"Pok\xC3\xA9mon\"\nicon = \"\"\n[[show.episode]]\nseason = 1\nepisode = 1\n"
        "title = \"\"\ndescription = \"\"\npath = \"p.mpg\"\nduration_ms = 1000\n";
    ctw_db *db = ctw_db_new();
    ctw_issues is = {0};
    const ctw_item *it;
    ASSERT_EQ(0, ctw_library_parse(db, lib, &is));
    ASSERT_EQ(0, ctw_config_parse(db, CN_ONE_SLOT(
        "{ type = \"episode\", show = \"Pok\xC3\xA9mon\", season = 1, episode = 1 }"), &is));
    it = &ctw_db_channel(db, 1)->slots[0].items[0];
    ASSERT(it->show == ctw_db_show(db, "Cartoon Network", "Pok\xC3\xA9mon"));
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

SUITE(config_suite)
{
    RUN_TEST(config_defaults);
    RUN_TEST(config_icon_relative_to_folder);
    RUN_TEST(config_empty_config);
    RUN_TEST(config_channels_sorted);
    RUN_TEST(config_slots_sorted);
    RUN_TEST(config_items_resolved);
    RUN_TEST(config_item_defaults);
    RUN_TEST(config_episode_item);
    RUN_TEST(config_episode_seasonless);
    RUN_TEST(config_episode_season_must_match);
    RUN_TEST(config_random_season_zero_is_all);
    RUN_TEST(config_from_other_channel);
    RUN_TEST(config_specific_ad);
    RUN_TEST(config_dup_channel);
    RUN_TEST(config_dup_slot);
    RUN_TEST(config_unknown_show);
    RUN_TEST(config_unknown_show_in_loop_indexes);
    RUN_TEST(config_unknown_show_from);
    RUN_TEST(config_missing_episode);
    RUN_TEST(config_unknown_ad);
    RUN_TEST(config_fill_in_loop);
    RUN_TEST(config_timed_without_slots);
    RUN_TEST(config_bad_time);
    RUN_TEST(config_bad_type);
    RUN_TEST(config_bad_mode);
    RUN_TEST(config_empty_pool_error);
    RUN_TEST(config_empty_pool_run_and_fill);
    RUN_TEST(config_loop_on_timed);
    RUN_TEST(config_slot_on_looping);
    RUN_TEST(config_loop_missing);
    RUN_TEST(config_loop_empty);
    RUN_TEST(config_number_range);
    RUN_TEST(config_item_ranges);
    RUN_TEST(config_item_range_edges);
    RUN_TEST(config_seed_range);
    RUN_TEST(config_version);
    RUN_TEST(config_syntax_error_line);
    RUN_TEST(config_missing_fields);
    RUN_TEST(config_wrong_typed_keys);
    RUN_TEST(config_wrong_typed_item_keys);
    RUN_TEST(config_collects_all_errors);
    RUN_TEST(config_channel_not_table);
    RUN_TEST(config_channel_wrong_top_level);
    RUN_TEST(config_slot_not_table);
    RUN_TEST(config_slot_wrong_type);
    RUN_TEST(config_items_not_array);
    RUN_TEST(config_items_missing);
    RUN_TEST(config_item_not_table);
    RUN_TEST(config_loop_not_array);
    RUN_TEST(config_loop_item_not_table);
    RUN_TEST(config_strings_survive_input_free);
    RUN_TEST(config_requires_library);
    RUN_TEST(config_crlf_and_bom);
    RUN_TEST(config_unicode_show_name);
}
