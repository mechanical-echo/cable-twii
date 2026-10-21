#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "helpers.h"

#define LIB_PATH "tests/fixtures/lib_basic.toml"

static char *read_file(const char *path, const char *extra)
{
    FILE *f = fopen(path, "rb");
    long n;
    size_t extra_len = extra ? strlen(extra) : 0;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + extra_len + 1);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    if (buf) {
        if (extra_len) memcpy(buf + n, extra, extra_len);
        buf[(size_t)n + extra_len] = '\0';
    }
    fclose(f);
    return buf;
}

ctw_db *load_library(const char *extra)
{
    ctw_issues is = {0};
    char *text = read_file(LIB_PATH, extra);
    ctw_db *db = ctw_db_new();
    int rc = text ? ctw_library_parse(db, text, &is) : -1;
    free(text);
    ctw_issues_free(&is);
    if (rc) {
        ctw_db_free(db);
        return NULL;
    }
    return db;
}

ctw_db *load_config(const char *cfg)
{
    ctw_issues is = {0};
    ctw_db *db = load_library(NULL);
    if (db && ctw_config_parse(db, cfg, &is)) {
        ctw_db_free(db);
        db = NULL;
    }
    ctw_issues_free(&is);
    return db;
}

int build_lineup(const char *cfg, int channel, ctw_date day, ctw_lineup *l)
{
    ctw_db *db = load_config(cfg);
    int rc;
    if (!db) return -99;
    rc = ctw_lineup_build(db, ctw_db_channel(db, channel), day, l);
    ctw_db_free(db);
    return rc;
}

enum greatest_test_res assert_contiguous(const ctw_lineup *l, int64_t from, int64_t to)
{
    ASSERT(l->count > 0);
    ASSERT_EQ_FMT(from, l->items[0].start_ms, "%lld");
    for (size_t i = 1; i < l->count; i++) {
        int64_t prev_end = l->items[i - 1].start_ms + (int64_t)l->items[i - 1].duration_ms;
        ASSERT_EQ_FMT(prev_end, l->items[i].start_ms, "%lld");
    }
    ASSERT_EQ_FMT(to, l->items[l->count - 1].start_ms + (int64_t)l->items[l->count - 1].duration_ms,
                  "%lld");
    PASS();
}

int has_warning(const ctw_lineup *l, int channel, ctw_code code, int slot, int item)
{
    for (size_t i = 0; i < l->warnings.count; i++) {
        const ctw_issue *w = &l->warnings.items[i];
        if (w->code == code && w->slot == slot && w->item == item && w->channel == channel &&
            w->level == CTW_LEVEL_WARNING && w->line == 0 && strcmp(w->file, "lineup") == 0) {
            return 1;
        }
    }
    return 0;
}

size_t count_warnings(const ctw_lineup *l, ctw_code code)
{
    size_t n = 0;
    for (size_t i = 0; i < l->warnings.count; i++) {
        if (l->warnings.items[i].code == code) n++;
    }
    return n;
}
