#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "tomlc99/toml.h"

#define LIB_FILE "library"

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

static void parse_ads(ctw_pctx *pc, const toml_array_t *arr)
{
    int n = toml_array_nelem(arr);
    int i;
    ctw_ad *ads = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *ads);
    size_t count = 0;
    if (!ads) {
        ctw_pctx_oom(pc);
        return;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[40];
        ctw_ad ad;
        int64_t dur;
        int failed = 0;
        snprintf(where, sizeof where, "ad #%d", i + 1);
        if (!t) {
            ctw_pctx_not_table(pc, where);
            continue;
        }
        if (ctw_get_str(pc, t, "path", 1, &ad.path, where)) failed = 1;
        if (ctw_get_str(pc, t, "channel", 1, &ad.channel_folder, where)) failed = 1;
        if (ctw_get_int(pc, t, "duration_ms", 1, 4294967295LL, &dur, where)) failed = 1;
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

static void parse_episodes(ctw_pctx *pc, ctw_show *show, const toml_array_t *arr, const char *swhere)
{
    int n = toml_array_nelem(arr);
    int i;
    ctw_episode *eps = ctw_arena_alloc(&pc->db->arena, (size_t)(n ? n : 1) * sizeof *eps);
    size_t count = 0;
    if (!eps) {
        ctw_pctx_oom(pc);
        return;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[200];
        ctw_episode ep;
        int64_t season, episode, dur;
        int failed = 0;
        snprintf(where, sizeof where, "%s episode #%d", swhere, i + 1);
        if (!t) {
            ctw_pctx_not_table(pc, where);
            continue;
        }
        if (ctw_get_int(pc, t, "season", 0, 65535, &season, where)) failed = 1;
        if (ctw_get_int(pc, t, "episode", 1, 65535, &episode, where)) failed = 1;
        if (ctw_get_str(pc, t, "path", 1, &ep.path, where)) failed = 1;
        if (ctw_get_str(pc, t, "title", 0, &ep.title, where)) failed = 1;
        if (ctw_get_str(pc, t, "description", 0, &ep.description, where)) failed = 1;
        if (ctw_get_int(pc, t, "duration_ms", 1, 4294967295LL, &dur, where)) failed = 1;
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

static void parse_shows(ctw_pctx *pc, const toml_array_t *arr)
{
    int n = toml_array_nelem(arr);
    int i;
    ctw_show *shows = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *shows);
    size_t count = 0;
    if (!shows) {
        ctw_pctx_oom(pc);
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
        if (!t) {
            ctw_pctx_not_table(pc, where);
            continue;
        }
        if (ctw_get_str(pc, t, "channel", 1, &s.channel_folder, where)) failed = 1;
        if (ctw_get_str(pc, t, "folder", 1, &s.folder, where)) failed = 1;
        if (!failed) snprintf(where, sizeof where, "show %s/%s", s.channel_folder, s.folder);
        if (ctw_get_str(pc, t, "name", 1, &s.name, where)) failed = 1;
        if (ctw_get_str(pc, t, "icon", 1, &s.icon, where)) failed = 1;
        eps = toml_array_in(t, "episode");
        if (!eps) {
            ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: missing episode", where);
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

int ctw_library_parse(ctw_db *db, const char *toml_text, ctw_issues *issues)
{
    toml_table_t *root;
    toml_array_t *arr;
    ctw_pctx pc;
    ctw_pctx_init(&pc, db, issues, LIB_FILE, CTW_E_BAD_VALUE);
    root = ctw_toml_open(&pc, toml_text);
    if (!root) return -1;
    ctw_check_version(&pc, root);
    arr = toml_array_in(root, "ad");
    if (arr) parse_ads(&pc, arr);
    else if (toml_key_exists(root, "ad")) ctw_pctx_wrong_top_level(&pc, "ad");
    arr = toml_array_in(root, "show");
    if (arr) parse_shows(&pc, arr);
    else if (toml_key_exists(root, "show")) ctw_pctx_wrong_top_level(&pc, "show");
    toml_free(root);
    return pc.bad ? -1 : 0;
}
