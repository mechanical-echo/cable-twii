#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"
#include "helpers.h"

#define DB_ROOT "tests/fixtures/root_basic"
#define DB_TMP_PATH 512

static int db_copy(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out;
    char buf[4096];
    size_t n;
    int rc = 0;
    if (!in) return -1;
    out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) rc = -1;
    fclose(in);
    if (fclose(out)) rc = -1;
    return rc;
}

static int db_make_parents(const char *root, const char *rel)
{
    char path[DB_TMP_PATH];
    size_t i;
    snprintf(path, sizeof path, "%s/%s", root, rel);
    for (i = strlen(root) + 1; path[i]; i++) {
        if (path[i] == '/') {
            path[i] = '\0';
            if (mkdir(path, 0700) != 0 && errno != EEXIST) return -1;
            path[i] = '/';
        }
    }
    return 0;
}

static int db_touch(const char *root, const char *rel)
{
    char path[DB_TMP_PATH];
    FILE *f;
    if (db_make_parents(root, rel)) return -1;
    snprintf(path, sizeof path, "%s/%s", root, rel);
    f = fopen(path, "wb");
    if (!f) return -1;
    return fclose(f);
}

static int db_write(const char *root, const char *name, const char *data, size_t len)
{
    char path[DB_TMP_PATH];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", root, name);
    f = fopen(path, "wb");
    if (!f) return -1;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    return fclose(f);
}

static void db_remove(const char *root, const char *rel)
{
    char path[DB_TMP_PATH];
    char *slash;
    snprintf(path, sizeof path, "%s/%s", root, rel);
    remove(path);
    while ((slash = strrchr(path, '/')) != NULL && strlen(path) > strlen(root)) {
        *slash = '\0';
        if (rmdir(path) != 0) break;
    }
}

static int db_tmp_root(char *out, size_t n)
{
    const char *base = getenv("TMPDIR");
    int w;
    if (!base || !*base) base = "/tmp";
    w = snprintf(out, n, "%s/ctw-db-XXXXXX", base);
    if (w < 0 || (size_t)w >= n) return -1;
    return mkdtemp(out) ? 0 : -1;
}

static void db_cleanup(const ctw_db *db, const char *root)
{
    size_t i, j;
    char path[DB_TMP_PATH];
    if (db) {
        for (i = 0; i < ctw_db_show_count(db); i++) {
            const ctw_show *s = ctw_db_show_at(db, i);
            for (j = 0; j < s->episode_count; j++) db_remove(root, s->episodes[j].path);
        }
        for (i = 0; i < ctw_db_ad_count(db); i++) db_remove(root, ctw_db_ad_at(db, i)->path);
    }
    snprintf(path, sizeof path, "%s/library.toml", root);
    remove(path);
    snprintf(path, sizeof path, "%s/config.toml", root);
    remove(path);
    rmdir(root);
}

static int db_has_issue(const ctw_issues *is, ctw_code code, const char *file, const char *needle)
{
    size_t i;
    for (i = 0; i < is->count; i++) {
        const ctw_issue *it = &is->items[i];
        if (it->code == code && it->level == CTW_LEVEL_ERROR && strcmp(it->file, file) == 0 &&
            it->channel == -1 && it->slot == -1 && it->item == -1 && it->line == 0 &&
            strstr(it->message, needle)) {
            return 1;
        }
    }
    return 0;
}

static int db_stage_root(char *root, size_t n, int library, int config)
{
    char dst[DB_TMP_PATH];
    if (db_tmp_root(root, n)) return -1;
    if (library) {
        snprintf(dst, sizeof dst, "%s/library.toml", root);
        if (db_copy(DB_ROOT "/library.toml", dst)) return -1;
    }
    if (config) {
        snprintf(dst, sizeof dst, "%s/config.toml", root);
        if (db_copy(DB_ROOT "/config.toml", dst)) return -1;
    }
    return 0;
}

TEST load_root_basic(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    const ctw_channel *cn;
    const ctw_channel *mtv;
    ASSERT_EQ(0, ctw_db_load(DB_ROOT, &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(0u, is.count);
    ASSERT_EQ(2u, ctw_db_channel_count(db));
    cn = ctw_db_channel(db, 1);
    mtv = ctw_db_channel(db, 2);
    ASSERT_STR_EQ("Cartoon Network", cn->name);
    ASSERT_STR_EQ("MTV", mtv->name);
    ASSERT_EQ(CTW_MODE_TIMED, cn->mode);
    ASSERT_EQ(CTW_MODE_LOOPING, mtv->mode);
    ASSERT_EQ(2u, cn->slot_count);
    ASSERT_EQ(6u, cn->slots[0].item_count);
    ASSERT_EQ(CTW_ITEM_RANDOM_RUN, cn->slots[0].items[0].type);
    ASSERT_EQ(3u, cn->slots[0].items[0].count);
    ASSERT_EQ(CTW_ITEM_ADS, cn->slots[0].items[1].type);
    ASSERT_EQ(CTW_ITEM_RANDOM, cn->slots[0].items[2].type);
    ASSERT_EQ(CTW_ITEM_ADS, cn->slots[0].items[3].type);
    ASSERT_EQ(CTW_ITEM_EPISODE, cn->slots[0].items[4].type);
    ASSERT_EQ(CTW_ITEM_RANDOM, cn->slots[0].items[5].type);
    ASSERT_EQ(4u, mtv->loop_count);
    ASSERT(!mtv->ads);
    ASSERT_EQ(1u, ctw_db_seed(db));
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST load_missing_config(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    char root[DB_TMP_PATH];
    int staged = db_stage_root(root, sizeof root, 1, 0);
    int rc = -2;
    size_t count = 99;
    int found = 0;
    int exact = 0;
    size_t channels = 99;
    size_t shows = 0;
    if (staged == 0) {
        rc = ctw_db_load(root, &db, &is);
        count = is.count;
        found = db_has_issue(&is, CTW_E_PARSE, "config", "config.toml not found");
        exact = count > 0 && strcmp(is.items[0].message, "config.toml not found") == 0;
        channels = db ? ctw_db_channel_count(db) : 99;
        shows = db ? ctw_db_show_count(db) : 0;
    }
    db_cleanup(NULL, root);
    ctw_issues_free(&is);
    ctw_db_free(db);
    ASSERT_EQ(0, staged);
    ASSERT_EQ(-1, rc);
    ASSERT_EQ(1u, count);
    ASSERT(found);
    ASSERT(exact);
    ASSERT_EQ(0u, channels);
    ASSERT(shows > 0);
    PASS();
}

TEST load_missing_library(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    char root[DB_TMP_PATH];
    int staged = db_stage_root(root, sizeof root, 0, 1);
    int rc = -2;
    size_t count = 99;
    int found = 0;
    int exact = 0;
    if (staged == 0) {
        rc = ctw_db_load(root, &db, &is);
        count = is.count;
        found = db_has_issue(&is, CTW_E_PARSE, "library", "library.toml not found");
        exact = count > 0 && strcmp(is.items[0].message, "library.toml not found") == 0;
    }
    db_cleanup(NULL, root);
    ctw_issues_free(&is);
    ASSERT(staged == 0 ? db != NULL : 1);
    ctw_db_free(db);
    ASSERT_EQ(0, staged);
    ASSERT_EQ(-1, rc);
    ASSERT_EQ(1u, count);
    ASSERT(found);
    ASSERT(exact);
    PASS();
}

TEST load_trailing_slash(void)
{
    ctw_issues a = {0};
    ctw_issues b = {0};
    ctw_db *da = NULL;
    ctw_db *dbs = NULL;
    ASSERT_EQ(0, ctw_db_load(DB_ROOT, &da, &a));
    ASSERT_EQ(0, ctw_db_load(DB_ROOT "/", &dbs, &b));
    ASSERT_EQ(a.count, b.count);
    ASSERT_EQ(ctw_db_channel_count(da), ctw_db_channel_count(dbs));
    ASSERT_EQ(ctw_db_show_count(da), ctw_db_show_count(dbs));
    ASSERT_EQ(ctw_db_ad_count(da), ctw_db_ad_count(dbs));
    ctw_issues_free(&a);
    ctw_issues_free(&b);
    ctw_db_free(da);
    ctw_db_free(dbs);
    PASS();
}

TEST load_missing_root(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    ASSERT_EQ(-1, ctw_db_load("/nonexistent/ctw-db-no-such-root", &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(1u, is.count);
    ASSERT_STR_EQ("library.toml not found", is.items[0].message);
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST load_path_too_long(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    char root[1100];
    memset(root, 'a', sizeof root - 1);
    root[sizeof root - 1] = '\0';
    ASSERT_EQ(-1, ctw_db_load(root, &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(1u, is.count);
    ASSERT(db_has_issue(&is, CTW_E_PARSE, "library", "path too long: library.toml"));
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST load_nul_byte(void)
{
    ctw_issues is = {0};
    ctw_issues is2 = {0};
    ctw_db *db = NULL;
    ctw_db *db2 = NULL;
    char root[DB_TMP_PATH];
    int staged = db_tmp_root(root, sizeof root);
    int wrote = -1;
    int rc = -2;
    int rc2 = -2;
    size_t count = 99;
    size_t count2 = 99;
    int found = 0;
    int found2 = 0;
    if (staged == 0) {
        wrote = db_write(root, "library.toml", "version = 1\n\0x = 2\n", 18);
        rc = ctw_db_load(root, &db, &is);
        count = is.count;
        found = db_has_issue(&is, CTW_E_PARSE, "library", "library.toml contains a NUL byte");
        {
            char dst[DB_TMP_PATH];
            snprintf(dst, sizeof dst, "%s/library.toml", root);
            wrote = db_copy(DB_ROOT "/library.toml", dst);
        }
        if (wrote == 0) wrote = db_write(root, "config.toml", "version = 1\n\0seed = 1\n", 20);
        rc2 = ctw_db_load(root, &db2, &is2);
        count2 = is2.count;
        found2 = db_has_issue(&is2, CTW_E_PARSE, "config", "config.toml contains a NUL byte");
    }
    db_cleanup(NULL, root);
    ctw_issues_free(&is);
    ctw_issues_free(&is2);
    ctw_db_free(db);
    ctw_db_free(db2);
    ASSERT_EQ(0, staged);
    ASSERT_EQ(0, wrote);
    ASSERT_EQ(-1, rc);
    ASSERT_EQ(1u, count);
    ASSERT(found);
    ASSERT_EQ(-1, rc2);
    ASSERT_EQ(1u, count2);
    ASSERT(found2);
    PASS();
}

TEST check_files_ok(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    ASSERT_EQ(0, ctw_db_load(DB_ROOT, &db, &is));
    ASSERT_EQ(0, ctw_db_check_files(db, DB_ROOT, &is));
    ASSERT_EQ(0, ctw_db_check_files(db, DB_ROOT "/", &is));
    ASSERT_EQ(0u, is.count);
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST check_files_missing(void)
{
    ctw_issues is = {0};
    ctw_issues chk = {0};
    ctw_db *db = NULL;
    char root[DB_TMP_PATH];
    const char *missing = "Cartoon Network/Ben 10/Season 1/S01E05 - Ben Ten.mpg";
    const char *missing_ad = "ads/ad-b.mpg";
    int staged = db_stage_root(root, sizeof root, 1, 1);
    int loaded = -2;
    int touched = 0;
    int rc = -2;
    size_t count = 99;
    int first = 0;
    int second = 0;
    size_t i, j;
    if (staged == 0) {
        loaded = ctw_db_load(root, &db, &is);
        for (i = 0; loaded == 0 && i < ctw_db_show_count(db); i++) {
            const ctw_show *s = ctw_db_show_at(db, i);
            for (j = 0; j < s->episode_count; j++)
                if (strcmp(s->episodes[j].path, missing) != 0 &&
                    db_touch(root, s->episodes[j].path)) touched = -1;
        }
        for (i = 0; loaded == 0 && i < ctw_db_ad_count(db); i++)
            if (strcmp(ctw_db_ad_at(db, i)->path, missing_ad) != 0 &&
                db_touch(root, ctw_db_ad_at(db, i)->path)) touched = -1;
        rc = ctw_db_check_files(db, root, &chk);
        count = chk.count;
        if (count == 2) {
            first = chk.items[0].code == CTW_E_MISSING_FILE && strstr(chk.items[0].message, missing);
            second = chk.items[1].code == CTW_E_MISSING_FILE &&
                     strstr(chk.items[1].message, missing_ad);
        }
    }
    db_cleanup(db, root);
    ctw_issues_free(&is);
    ctw_issues_free(&chk);
    ctw_db_free(db);
    ASSERT_EQ(0, staged);
    ASSERT_EQ(0, loaded);
    ASSERT_EQ(0, touched);
    ASSERT_EQ(-1, rc);
    ASSERT_EQ(2u, count);
    ASSERT(first);
    ASSERT(second);
    PASS();
}

TEST check_files_path_too_long(void)
{
    ctw_issues is = {0};
    ctw_issues chk = {0};
    ctw_db *db = ctw_db_new();
    char lib[2048];
    char longrel[1100];
    int rc;
    memset(longrel, 'b', sizeof longrel - 1);
    longrel[sizeof longrel - 1] = '\0';
    snprintf(lib, sizeof lib,
             "version = 1\ngenerated = 2026-10-20T14:00:00\n\n[[show]]\nchannel = \"c\"\n"
             "folder = \"s\"\nname = \"s\"\nicon = \"\"\n\n  [[show.episode]]\n  season = 1\n"
             "  episode = 1\n  title = \"t\"\n  description = \"\"\n  path = \"%s\"\n"
             "  duration_ms = 1000\n", longrel);
    ASSERT_EQ(0, ctw_library_parse(db, lib, &is));
    rc = ctw_db_check_files(db, DB_ROOT, &chk);
    ASSERT_EQ(-1, rc);
    ASSERT_EQ(1u, chk.count);
    ASSERT(db_has_issue(&chk, CTW_E_MISSING_FILE, "library", "path too long: bbbb"));
    ctw_issues_free(&is);
    ctw_issues_free(&chk);
    ctw_db_free(db);
    PASS();
}

TEST load_root_basic_lineups(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    ctw_date day = {2026, 10, 20};
    int n;
    ASSERT_EQ(0, ctw_db_load(DB_ROOT, &db, &is));
    for (n = 1; n <= 2; n++) {
        ctw_lineup l = {0};
        int64_t from = n == 1 ? 25200000 : 0;
        ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, n), day, &l));
        CHECK_CALL(assert_contiguous(&l, from, from + (int64_t)CTW_DAY_MS));
        ctw_lineup_free(&l);
    }
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

SUITE(db_suite)
{
    RUN_TEST(load_root_basic);
    RUN_TEST(load_missing_config);
    RUN_TEST(load_missing_library);
    RUN_TEST(load_trailing_slash);
    RUN_TEST(load_missing_root);
    RUN_TEST(load_path_too_long);
    RUN_TEST(load_nul_byte);
    RUN_TEST(check_files_ok);
    RUN_TEST(check_files_missing);
    RUN_TEST(check_files_path_too_long);
    RUN_TEST(load_root_basic_lineups);
}
