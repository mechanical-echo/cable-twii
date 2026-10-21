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

static int db_copy(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out;
    char buf[4096];
    size_t n;
    if (!in) return -1;
    out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 0;
}

static void db_make_parents(const char *root, const char *rel)
{
    char path[512];
    size_t i;
    snprintf(path, sizeof path, "%s/%s", root, rel);
    for (i = strlen(root) + 1; path[i]; i++) {
        if (path[i] == '/') {
            path[i] = '\0';
            mkdir(path, 0700);
            path[i] = '/';
        }
    }
}

static void db_touch(const char *root, const char *rel)
{
    char path[512];
    FILE *f;
    db_make_parents(root, rel);
    snprintf(path, sizeof path, "%s/%s", root, rel);
    f = fopen(path, "wb");
    if (f) fclose(f);
}

static void db_remove(const char *root, const char *rel)
{
    char path[512];
    char *slash;
    snprintf(path, sizeof path, "%s/%s", root, rel);
    remove(path);
    while ((slash = strrchr(path, '/')) != NULL && strlen(path) > strlen(root)) {
        *slash = '\0';
        if (rmdir(path) != 0) break;
    }
}

static void db_tmp_root(char *out, size_t n, const char *tag)
{
    snprintf(out, n, "/private/tmp/ctw-db-%s-%ld", tag, (long)getpid());
    mkdir(out, 0700);
}

static void db_remove_all(const ctw_db *db, const char *root)
{
    size_t i, j;
    char path[512];
    for (i = 0; i < ctw_db_show_count(db); i++) {
        const ctw_show *s = ctw_db_show_at(db, i);
        for (j = 0; j < s->episode_count; j++) db_remove(root, s->episodes[j].path);
    }
    for (i = 0; i < ctw_db_ad_count(db); i++) db_remove(root, ctw_db_ad_at(db, i)->path);
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

TEST load_root_basic(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    ASSERT_EQ(0, ctw_db_load(DB_ROOT, &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(0u, is.count);
    ASSERT_EQ(2u, ctw_db_channel_count(db));
    ASSERT_STR_EQ("Cartoon Network", ctw_db_channel(db, 1)->name);
    ASSERT_STR_EQ("MTV", ctw_db_channel(db, 2)->name);
    ASSERT_EQ(CTW_MODE_TIMED, ctw_db_channel(db, 1)->mode);
    ASSERT_EQ(CTW_MODE_LOOPING, ctw_db_channel(db, 2)->mode);
    ASSERT_EQ(2u, ctw_db_channel(db, 1)->slot_count);
    ASSERT_EQ(4u, ctw_db_channel(db, 2)->loop_count);
    ASSERT_EQ(1u, ctw_db_seed(db));
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST load_missing_config(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    char root[128];
    char dst[256];
    db_tmp_root(root, sizeof root, "mc");
    snprintf(dst, sizeof dst, "%s/library.toml", root);
    ASSERT_EQ(0, db_copy(DB_ROOT "/library.toml", dst));
    ASSERT_EQ(-1, ctw_db_load(root, &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(1u, is.count);
    ASSERT(db_has_issue(&is, CTW_E_PARSE, "config", "config.toml not found"));
    ASSERT_STR_EQ("config.toml not found", is.items[0].message);
    ASSERT_EQ(0u, ctw_db_channel_count(db));
    ASSERT(ctw_db_show_count(db) > 0);
    remove(dst);
    rmdir(root);
    ctw_issues_free(&is);
    ctw_db_free(db);
    PASS();
}

TEST load_missing_library(void)
{
    ctw_issues is = {0};
    ctw_db *db = NULL;
    char root[128];
    char dst[256];
    db_tmp_root(root, sizeof root, "ml");
    snprintf(dst, sizeof dst, "%s/config.toml", root);
    ASSERT_EQ(0, db_copy(DB_ROOT "/config.toml", dst));
    ASSERT_EQ(-1, ctw_db_load(root, &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(1u, is.count);
    ASSERT(db_has_issue(&is, CTW_E_PARSE, "library", "library.toml not found"));
    ASSERT_STR_EQ("library.toml not found", is.items[0].message);
    remove(dst);
    rmdir(root);
    ctw_issues_free(&is);
    ctw_db_free(db);
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
    ASSERT_EQ(-1, ctw_db_load("/private/tmp/ctw-db-no-such-root", &db, &is));
    ASSERT(db != NULL);
    ASSERT_EQ(1u, is.count);
    ASSERT_STR_EQ("library.toml not found", is.items[0].message);
    ctw_issues_free(&is);
    ctw_db_free(db);
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
    char root[128];
    char dst[256];
    const char *missing = "Cartoon Network/Ben 10/Season 1/S01E05 - Ben Ten.mpg";
    size_t i, j;
    db_tmp_root(root, sizeof root, "cf");
    snprintf(dst, sizeof dst, "%s/library.toml", root);
    ASSERT_EQ(0, db_copy(DB_ROOT "/library.toml", dst));
    snprintf(dst, sizeof dst, "%s/config.toml", root);
    ASSERT_EQ(0, db_copy(DB_ROOT "/config.toml", dst));
    ASSERT_EQ(0, ctw_db_load(root, &db, &is));
    for (i = 0; i < ctw_db_show_count(db); i++) {
        const ctw_show *s = ctw_db_show_at(db, i);
        for (j = 0; j < s->episode_count; j++)
            if (strcmp(s->episodes[j].path, missing) != 0) db_touch(root, s->episodes[j].path);
    }
    for (i = 0; i < ctw_db_ad_count(db); i++) db_touch(root, ctw_db_ad_at(db, i)->path);
    ASSERT_EQ(-1, ctw_db_check_files(db, root, &chk));
    ASSERT_EQ(1u, chk.count);
    ASSERT(db_has_issue(&chk, CTW_E_MISSING_FILE, "library", missing));
    db_remove_all(db, root);
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
        ASSERT_EQ(0, ctw_lineup_build(db, ctw_db_channel(db, n), day, &l));
        CHECK_CALL(assert_contiguous(&l, n == 1 ? 25200000 : 0, (n == 1 ? 25200000 : 0) + (int64_t)CTW_DAY_MS));
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
    RUN_TEST(check_files_ok);
    RUN_TEST(check_files_missing);
    RUN_TEST(load_root_basic_lineups);
}
