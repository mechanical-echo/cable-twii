#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "tomlc99/toml.h"

#define LIB_FILE "library"
#define LIB_ERR(code, line, ...) \
    ctw_issues_add(issues, CTW_LEVEL_ERROR, (code), -1, -1, -1, (line), LIB_FILE, __VA_ARGS__)

typedef struct {
    ctw_db *db;
    ctw_issues *issues;
    int bad;
} parse_ctx;

ctw_db *ctw_db_new(void)
{
    ctw_db *db = calloc(1, sizeof *db);
    if (db) ctw_arena_init(&db->arena);
    return db;
}

void ctw_db_free(ctw_db *db)
{
    if (!db) return;
    ctw_arena_free(&db->arena);
    free(db);
}

const ctw_show *ctw_db_show(const ctw_db *db, const char *channel_folder, const char *folder)
{
    size_t i;
    for (i = 0; i < db->show_count; i++) {
        const ctw_show *s = &db->shows[i];
        if (strcmp(s->channel_folder, channel_folder) == 0 && strcmp(s->folder, folder) == 0)
            return s;
    }
    return NULL;
}

size_t ctw_db_show_count(const ctw_db *db)
{
    return db->show_count;
}

const ctw_show *ctw_db_show_at(const ctw_db *db, size_t i)
{
    return i < db->show_count ? &db->shows[i] : NULL;
}

size_t ctw_db_ad_count(const ctw_db *db)
{
    return db->ad_count;
}

const ctw_ad *ctw_db_ad_at(const ctw_db *db, size_t i)
{
    return i < db->ad_count ? &db->ads[i] : NULL;
}

static int get_int(const toml_table_t *t, const char *key, int64_t lo, int64_t hi, int64_t *out,
                   const char *where, parse_ctx *pc)
{
    toml_datum_t d;
    if (!toml_key_exists(t, key)) {
        ctw_issues_add(pc->issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, LIB_FILE,
                       "%s: missing %s", where, key);
        return -1;
    }
    d = toml_int_in(t, key);
    if (!d.ok || d.u.i < lo || d.u.i > hi) {
        ctw_issues_add(pc->issues, CTW_LEVEL_ERROR, CTW_E_BAD_VALUE, -1, -1, -1, 0, LIB_FILE,
                       "%s: %s must be an integer from %lld to %lld", where, key,
                       (long long)lo, (long long)hi);
        return -1;
    }
    *out = d.u.i;
    return 0;
}

static int get_str(const toml_table_t *t, const char *key, int required, const char **out,
                   const char *where, parse_ctx *pc)
{
    toml_datum_t d;
    if (!toml_key_exists(t, key)) {
        if (!required) {
            *out = "";
            return 0;
        }
        ctw_issues_add(pc->issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, LIB_FILE,
                       "%s: missing %s", where, key);
        return -1;
    }
    d = toml_string_in(t, key);
    if (!d.ok) {
        ctw_issues_add(pc->issues, CTW_LEVEL_ERROR, CTW_E_BAD_VALUE, -1, -1, -1, 0, LIB_FILE,
                       "%s: %s must be a string", where, key);
        return -1;
    }
    *out = ctw_arena_strdup(&pc->db->arena, d.u.s);
    free(d.u.s);
    if (!*out) return -1;
    return 0;
}

static int episode_less(const ctw_episode *a, const ctw_episode *b)
{
    if (a->season != b->season) return a->season < b->season;
    return a->episode < b->episode;
}

static void sort_episodes(ctw_episode *e, ctw_episode *tmp, size_t n)
{
    size_t mid, i, j, k;
    if (n < 2) return;
    mid = n / 2;
    sort_episodes(e, tmp, mid);
    sort_episodes(e + mid, tmp, n - mid);
    i = 0;
    j = mid;
    k = 0;
    while (i < mid && j < n) {
        if (episode_less(&e[j], &e[i])) tmp[k++] = e[j++];
        else tmp[k++] = e[i++];
    }
    while (i < mid) tmp[k++] = e[i++];
    while (j < n) tmp[k++] = e[j++];
    memcpy(e, tmp, n * sizeof *e);
}

static void parse_ads(parse_ctx *pc, const toml_array_t *arr)
{
    int n = toml_array_nelem(arr);
    int i;
    ctw_ad *ads = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *ads);
    size_t count = 0;
    if (!ads) {
        pc->bad = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[40];
        ctw_ad ad;
        int64_t dur;
        int failed = 0;
        snprintf(where, sizeof where, "ad #%d", i + 1);
        if (get_str(t, "path", 1, &ad.path, where, pc)) failed = 1;
        if (get_str(t, "channel", 1, &ad.channel_folder, where, pc)) failed = 1;
        if (get_int(t, "duration_ms", 1, 4294967295LL, &dur, where, pc)) failed = 1;
        if (failed) {
            pc->bad = 1;
            continue;
        }
        ad.duration_ms = (uint32_t)dur;
        ads[count++] = ad;
    }
    pc->db->ads = ads;
    pc->db->ad_count = count;
}

static void parse_episodes(parse_ctx *pc, ctw_show *show, const toml_array_t *arr, const char *swhere)
{
    int n = toml_array_nelem(arr);
    int i;
    ctw_episode *eps = ctw_arena_alloc(&pc->db->arena, (size_t)(n ? n : 1) * sizeof *eps);
    size_t count = 0;
    if (!eps) {
        pc->bad = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[200];
        ctw_episode ep;
        int64_t season, episode, dur;
        int failed = 0;
        snprintf(where, sizeof where, "%s episode #%d", swhere, i + 1);
        if (get_int(t, "season", 0, 65535, &season, where, pc)) failed = 1;
        if (get_int(t, "episode", 1, 65535, &episode, where, pc)) failed = 1;
        if (get_str(t, "path", 1, &ep.path, where, pc)) failed = 1;
        if (get_str(t, "title", 0, &ep.title, where, pc)) failed = 1;
        if (get_str(t, "description", 0, &ep.description, where, pc)) failed = 1;
        if (get_int(t, "duration_ms", 1, 4294967295LL, &dur, where, pc)) failed = 1;
        if (failed) {
            pc->bad = 1;
            continue;
        }
        ep.season = (uint16_t)season;
        ep.episode = (uint16_t)episode;
        ep.duration_ms = (uint32_t)dur;
        eps[count++] = ep;
    }
    if (count > 1) {
        ctw_episode *tmp = malloc(count * sizeof *tmp);
        if (tmp) {
            sort_episodes(eps, tmp, count);
            free(tmp);
        } else {
            size_t a, b;
            for (a = 1; a < count; a++) {
                ctw_episode cur = eps[a];
                for (b = a; b > 0 && episode_less(&cur, &eps[b - 1]); b--) eps[b] = eps[b - 1];
                eps[b] = cur;
            }
        }
    }
    show->episodes = eps;
    show->episode_count = count;
}

static void parse_shows(parse_ctx *pc, const toml_array_t *arr)
{
    int n = toml_array_nelem(arr);
    int i;
    ctw_show *shows = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *shows);
    size_t count = 0;
    if (!shows) {
        pc->bad = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[200];
        ctw_show s;
        toml_array_t *eps;
        int failed = 0;
        memset(&s, 0, sizeof s);
        snprintf(where, sizeof where, "show #%d", i + 1);
        if (get_str(t, "channel", 1, &s.channel_folder, where, pc)) failed = 1;
        if (get_str(t, "folder", 1, &s.folder, where, pc)) failed = 1;
        if (!failed) snprintf(where, sizeof where, "show %s/%s", s.channel_folder, s.folder);
        if (get_str(t, "name", 1, &s.name, where, pc)) failed = 1;
        if (get_str(t, "icon", 1, &s.icon, where, pc)) failed = 1;
        eps = toml_array_in(t, "episode");
        if (!eps) {
            ctw_issues_add(pc->issues, CTW_LEVEL_ERROR, CTW_E_PARSE, -1, -1, -1, 0, LIB_FILE,
                           "%s: missing episode", where);
            failed = 1;
        } else {
            parse_episodes(pc, &s, eps, where);
        }
        if (failed) {
            pc->bad = 1;
            continue;
        }
        shows[count++] = s;
    }
    pc->db->shows = shows;
    pc->db->show_count = count;
}

static int parse_error_line(const char *errbuf)
{
    int line = 0;
    if (sscanf(errbuf, "line %d:", &line) == 1 && line > 0) return line;
    return 0;
}

static void check_version(parse_ctx *pc, const toml_table_t *root)
{
    ctw_issues *issues = pc->issues;
    toml_datum_t d;
    if (!toml_key_exists(root, "version")) {
        LIB_ERR(CTW_E_PARSE, 0, "missing version");
        pc->bad = 1;
        return;
    }
    d = toml_int_in(root, "version");
    if (!d.ok || d.u.i != 1) {
        LIB_ERR(CTW_E_VERSION, 0, "version must be 1");
        pc->bad = 1;
    }
}

int ctw_library_parse(ctw_db *db, const char *toml_text, ctw_issues *issues)
{
    char errbuf[200];
    char *copy;
    toml_table_t *root;
    toml_array_t *arr;
    parse_ctx pc;
    pc.db = db;
    pc.issues = issues;
    pc.bad = 0;
    copy = malloc(strlen(toml_text) + 1);
    if (!copy) {
        LIB_ERR(CTW_E_PARSE, 0, "out of memory");
        return -1;
    }
    strcpy(copy, toml_text);
    root = toml_parse(copy, errbuf, sizeof errbuf);
    free(copy);
    if (!root) {
        LIB_ERR(CTW_E_PARSE, parse_error_line(errbuf), "%s", errbuf);
        return -1;
    }
    check_version(&pc, root);
    arr = toml_array_in(root, "ad");
    if (arr) parse_ads(&pc, arr);
    arr = toml_array_in(root, "show");
    if (arr) parse_shows(&pc, arr);
    toml_free(root);
    return pc.bad ? -1 : 0;
}
