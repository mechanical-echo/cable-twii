#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"

#define CFG_FILE "config"

uint32_t ctw_db_seed(const ctw_db *db)
{
    return db->seed;
}

size_t ctw_db_channel_count(const ctw_db *db)
{
    return db->channel_count;
}

const ctw_channel *ctw_db_channel_at(const ctw_db *db, size_t i)
{
    return i < db->channel_count ? &db->channels[i] : NULL;
}

const ctw_channel *ctw_db_channel(const ctw_db *db, int number)
{
    size_t i;
    for (i = 0; i < db->channel_count; i++)
        if (db->channels[i].number == number) return &db->channels[i];
    return NULL;
}

static const ctw_show *resolve_show(ctw_pctx *pc, const toml_table_t *t, const ctw_channel *ch,
                                    const char *where, const char **show_name)
{
    const char *from = ch->folder;
    const ctw_show *show;
    int bad = pc->bad;
    if (ctw_get_str(pc, t, "show", 1, show_name, where)) return NULL;
    if (toml_key_exists(t, "from") && ctw_get_str(pc, t, "from", 1, &from, where)) return NULL;
    if (pc->bad != bad) return NULL;
    show = ctw_db_show(pc->db, from, *show_name);
    if (!show)
        ctw_pctx_error(pc, CTW_E_UNKNOWN_SHOW, 0, "%s: no show %s in %s", where, *show_name, from);
    return show;
}

static void check_pool(ctw_pctx *pc, const ctw_item *it, const char *where)
{
    size_t i;
    for (i = 0; i < it->show->episode_count; i++)
        if (it->season == 0 || it->show->episodes[i].season == it->season) return;
    ctw_pctx_error(pc, CTW_E_EMPTY_POOL, 0, "%s: no episodes for %s season %u", where,
                   it->show->folder, (unsigned)it->season);
}

static void parse_optional_int(ctw_pctx *pc, const toml_table_t *t, const char *key, int64_t lo,
                               int64_t hi, int64_t dflt, uint16_t *out, const char *where)
{
    int64_t v;
    if (!toml_key_exists(t, key)) {
        *out = (uint16_t)dflt;
        return;
    }
    if (ctw_get_int(pc, t, key, lo, hi, &v, where) == 0) *out = (uint16_t)v;
}

static void parse_episode_item(ctw_pctx *pc, const toml_table_t *t, const ctw_channel *ch,
                               ctw_item *it, const char *where)
{
    const char *name;
    int64_t season, episode;
    int bad = pc->bad;
    size_t i;
    int found = 0;
    it->show = resolve_show(pc, t, ch, where, &name);
    ctw_get_int(pc, t, "season", 0, 65535, &season, where);
    ctw_get_int(pc, t, "episode", 1, 65535, &episode, where);
    if (pc->bad != bad || !it->show) return;
    it->season = (uint16_t)season;
    it->episode = (uint16_t)episode;
    for (i = 0; i < it->show->episode_count && !found; i++)
        found = it->show->episodes[i].season == it->season &&
                it->show->episodes[i].episode == it->episode;
    if (!found)
        ctw_pctx_error(pc, CTW_E_MISSING_EPISODE, 0, "%s: %s has no season %d episode %d", where,
                       name, (int)season, (int)episode);
}

static void parse_random_item(ctw_pctx *pc, const toml_table_t *t, const ctw_channel *ch,
                              ctw_item *it, const char *where)
{
    const char *name;
    int bad;
    it->show = resolve_show(pc, t, ch, where, &name);
    bad = pc->bad;
    parse_optional_int(pc, t, "season", 0, 65535, 0, &it->season, where);
    parse_optional_int(pc, t, "count", 1, 65535, 1, &it->count, where);
    if (pc->bad == bad && it->show) check_pool(pc, it, where);
}

static void parse_fill_item(ctw_pctx *pc, const toml_table_t *t, const ctw_channel *ch,
                            ctw_item *it, const char *where)
{
    const char *name;
    int bad;
    it->show = resolve_show(pc, t, ch, where, &name);
    bad = pc->bad;
    parse_optional_int(pc, t, "season", 0, 65535, 0, &it->season, where);
    parse_optional_int(pc, t, "ads_between", 0, 65535, 0, &it->ads_between, where);
    if (pc->slot < 0)
        ctw_pctx_error(pc, CTW_E_FILL_IN_LOOP, 0, "%s: fill is not allowed in a looping channel",
                       where);
    if (pc->bad == bad && it->show) check_pool(pc, it, where);
}

static void parse_ad_item(ctw_pctx *pc, const toml_table_t *t, ctw_item *it, const char *where)
{
    const char *path;
    size_t i;
    if (ctw_get_str(pc, t, "path", 1, &path, where)) return;
    for (i = 0; i < pc->db->ad_count; i++) {
        if (strcmp(pc->db->ads[i].path, path) == 0) {
            it->ad = &pc->db->ads[i];
            return;
        }
    }
    ctw_pctx_error(pc, CTW_E_UNKNOWN_AD, 0, "%s: no ad %s in the library", where, path);
}

static void parse_item(ctw_pctx *pc, const toml_table_t *t, const ctw_channel *ch, ctw_item *it,
                       const char *where)
{
    const char *type;
    memset(it, 0, sizeof *it);
    it->count = 1;
    if (ctw_get_str(pc, t, "type", 1, &type, where)) return;
    if (strcmp(type, "episode") == 0) {
        it->type = CTW_ITEM_EPISODE;
        it->count = 0;
        parse_episode_item(pc, t, ch, it, where);
    } else if (strcmp(type, "random") == 0) {
        it->type = CTW_ITEM_RANDOM;
        parse_random_item(pc, t, ch, it, where);
    } else if (strcmp(type, "random-run") == 0) {
        it->type = CTW_ITEM_RANDOM_RUN;
        parse_random_item(pc, t, ch, it, where);
    } else if (strcmp(type, "ads") == 0) {
        it->type = CTW_ITEM_ADS;
        parse_optional_int(pc, t, "count", 1, 65535, 1, &it->count, where);
    } else if (strcmp(type, "ad") == 0) {
        it->type = CTW_ITEM_AD;
        it->count = 0;
        parse_ad_item(pc, t, it, where);
    } else if (strcmp(type, "fill") == 0) {
        it->type = CTW_ITEM_FILL;
        it->count = 0;
        parse_fill_item(pc, t, ch, it, where);
    } else {
        ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: unknown type %s", where, type);
    }
}

static ctw_item *parse_items(ctw_pctx *pc, const toml_array_t *arr, const ctw_channel *ch,
                             int in_slot, size_t *out_count)
{
    int n = toml_array_nelem(arr);
    int i;
    size_t count = 0;
    ctw_item *items = ctw_arena_alloc(&pc->db->arena, (size_t)(n ? n : 1) * sizeof *items);
    if (!items) {
        ctw_pctx_oom(pc);
        return NULL;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[120];
        int bad = pc->bad;
        pc->item = i;
        if (in_slot) {
            snprintf(where, sizeof where, "channel %d slot %d item %d", ch->number, pc->slot + 1,
                     i + 1);
        } else {
            snprintf(where, sizeof where, "channel %d loop item %d", ch->number, i + 1);
        }
        if (!t) {
            ctw_pctx_not_table(pc, where);
            continue;
        }
        parse_item(pc, t, ch, &items[count], where);
        if (pc->bad == bad) count++;
    }
    pc->item = -1;
    *out_count = count;
    return items;
}

static void parse_slot(ctw_pctx *pc, const toml_table_t *t, ctw_channel *ch, int index,
                       uint32_t *seen, size_t *seen_count)
{
    char where[80];
    const char *at;
    uint32_t at_ms = 0;
    int ok = 1;
    size_t j;
    toml_array_t *items;
    ctw_slot slot;
    int bad = pc->bad;
    snprintf(where, sizeof where, "channel %d slot %d", ch->number, index + 1);
    memset(&slot, 0, sizeof slot);
    if (ctw_get_str(pc, t, "at", 1, &at, where)) {
        ok = 0;
    } else if (ctw_parse_hhmm(at, &at_ms) != 0) {
        ctw_pctx_error(pc, CTW_E_BAD_TIME, 0, "%s: at must be HH:MM, got %s", where, at);
        ok = 0;
    } else {
        for (j = 0; j < *seen_count; j++) {
            if (seen[j] == at_ms) {
                ctw_pctx_error(pc, CTW_E_DUP_SLOT, 0, "%s: another slot already starts at %s",
                               where, at);
                ok = 0;
                break;
            }
        }
        if (ok) seen[(*seen_count)++] = at_ms;
    }
    items = toml_array_in(t, "items");
    if (!items) {
        if (toml_key_exists(t, "items"))
            ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: items must be an array", where);
        else
            ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: missing items", where);
        return;
    }
    slot.items = parse_items(pc, items, ch, 1, &slot.item_count);
    if (!ok || pc->bad != bad || !slot.items) return;
    slot.at_ms = at_ms;
    ch->slots[ch->slot_count++] = slot;
}

static uint64_t slot_key(const toml_table_t *t)
{
    toml_datum_t d = toml_string_in(t, "at");
    uint32_t ms;
    uint64_t key = (uint64_t)UINT32_MAX + 1;
    if (d.ok) {
        if (ctw_parse_hhmm(d.u.s, &ms) == 0) key = ms;
        free(d.u.s);
    }
    return key;
}

static int *slot_order(ctw_pctx *pc, const toml_array_t *arr, int n)
{
    int *order = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *order);
    uint64_t *keys = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *keys);
    int a, b;
    if (!order || !keys) {
        ctw_pctx_oom(pc);
        return NULL;
    }
    for (a = 0; a < n; a++) {
        toml_table_t *t = toml_table_at(arr, a);
        keys[a] = t ? slot_key(t) : (uint64_t)UINT32_MAX + 2;
    }
    for (a = 0; a < n; a++) {
        int cur = a;
        for (b = a; b > 0 && keys[cur] < keys[order[b - 1]]; b--) order[b] = order[b - 1];
        order[b] = cur;
    }
    return order;
}

static void parse_timed(ctw_pctx *pc, const toml_table_t *t, ctw_channel *ch, const char *where)
{
    toml_array_t *arr;
    int n, i;
    size_t seen_count = 0;
    uint32_t *seen;
    int *order;
    if (toml_key_exists(t, "loop"))
        ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: loop is not allowed on a timed channel", where);
    arr = toml_array_in(t, "slot");
    if (!arr) {
        if (toml_key_exists(t, "slot"))
            ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: slot must be an array of tables", where);
        else
            ctw_pctx_error(pc, CTW_E_NO_SLOTS, 0, "%s: a timed channel needs at least one slot",
                           where);
        return;
    }
    n = toml_array_nelem(arr);
    if (n == 0) {
        ctw_pctx_error(pc, CTW_E_NO_SLOTS, 0, "%s: a timed channel needs at least one slot", where);
        return;
    }
    ch->slots = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *ch->slots);
    seen = ctw_arena_alloc(&pc->db->arena, (size_t)n * sizeof *seen);
    if (!ch->slots || !seen) {
        ctw_pctx_oom(pc);
        return;
    }
    order = slot_order(pc, arr, n);
    if (!order) return;
    for (i = 0; i < n; i++) {
        toml_table_t *st = toml_table_at(arr, order[i]);
        char swhere[80];
        pc->slot = i;
        if (!st) {
            snprintf(swhere, sizeof swhere, "%s slot %d", where, i + 1);
            ctw_pctx_not_table(pc, swhere);
            continue;
        }
        parse_slot(pc, st, ch, i, seen, &seen_count);
    }
    pc->slot = -1;
}

static void parse_looping(ctw_pctx *pc, const toml_table_t *t, ctw_channel *ch, const char *where)
{
    toml_array_t *arr;
    if (toml_key_exists(t, "slot"))
        ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: slot is not allowed on a looping channel",
                       where);
    arr = toml_array_in(t, "loop");
    if (!arr) {
        if (toml_key_exists(t, "loop"))
            ctw_pctx_error(pc, CTW_E_PARSE, 0, "%s: loop must be an array of tables", where);
        else
            ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: a looping channel needs loop", where);
        return;
    }
    if (toml_array_nelem(arr) == 0) {
        ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: loop must not be empty", where);
        return;
    }
    ch->loop = parse_items(pc, arr, ch, 0, &ch->loop_count);
}

static const char *join_path(ctw_pctx *pc, const char *folder, const char *file)
{
    size_t fl = strlen(folder);
    size_t nl = strlen(file);
    char *p = ctw_arena_alloc(&pc->db->arena, fl + nl + 2);
    if (!p) {
        ctw_pctx_oom(pc);
        return NULL;
    }
    memcpy(p, folder, fl);
    p[fl] = '/';
    memcpy(p + fl + 1, file, nl + 1);
    return p;
}

static void parse_channel_fields(ctw_pctx *pc, const toml_table_t *t, ctw_channel *ch,
                                 const char *where, int *mode_ok)
{
    int64_t number;
    int ads = 0;
    const char *mode = "";
    const char *icon = "channel.png";
    *mode_ok = 0;
    if (ctw_get_int(pc, t, "number", 1, 999, &number, where) == 0) {
        ch->number = (int)number;
        pc->channel = ch->number;
    }
    ctw_get_str(pc, t, "name", 1, &ch->name, where);
    ctw_get_str(pc, t, "category", 1, &ch->category, where);
    if (!ch->name) ch->name = "";
    if (!ch->category) ch->category = "";
    ch->folder = ch->name;
    if (toml_key_exists(t, "folder")) ctw_get_str(pc, t, "folder", 1, &ch->folder, where);
    if (!ch->folder) ch->folder = ch->name;
    if (toml_key_exists(t, "icon") && ctw_get_str(pc, t, "icon", 1, &icon, where)) icon = "channel.png";
    ch->icon = join_path(pc, ch->folder, icon);
    if (ctw_get_bool(pc, t, "ads", &ads, where) == 0) ch->ads = ads != 0;
    if (ctw_get_str(pc, t, "mode", 1, &mode, where) == 0) {
        if (strcmp(mode, "timed") == 0) {
            ch->mode = CTW_MODE_TIMED;
            *mode_ok = 1;
        } else if (strcmp(mode, "looping") == 0) {
            ch->mode = CTW_MODE_LOOPING;
            *mode_ok = 1;
        } else {
            ctw_pctx_error(pc, CTW_E_BAD_VALUE, 0, "%s: mode must be timed or looping, got %s",
                           where, mode);
        }
    }
}

static void parse_channels(ctw_pctx *pc, const toml_array_t *arr)
{
    int n = toml_array_nelem(arr);
    int i;
    size_t count = 0;
    int *numbers;
    size_t seen = 0;
    ctw_channel *out = ctw_arena_alloc(&pc->db->arena, (size_t)(n ? n : 1) * sizeof *out);
    numbers = ctw_arena_alloc(&pc->db->arena, (size_t)(n ? n : 1) * sizeof *numbers);
    if (!out || !numbers) {
        ctw_pctx_oom(pc);
        return;
    }
    for (i = 0; i < n; i++) {
        toml_table_t *t = toml_table_at(arr, i);
        char where[40];
        ctw_channel ch;
        int mode_ok;
        int bad = pc->bad;
        int dup = 0;
        size_t j;
        snprintf(where, sizeof where, "channel #%d", i + 1);
        pc->channel = -1;
        if (!t) {
            ctw_pctx_not_table(pc, where);
            continue;
        }
        memset(&ch, 0, sizeof ch);
        parse_channel_fields(pc, t, &ch, where, &mode_ok);
        if (ch.number) {
            snprintf(where, sizeof where, "channel %d", ch.number);
            for (j = 0; j < seen; j++) dup |= numbers[j] == ch.number;
            if (dup)
                ctw_pctx_error(pc, CTW_E_DUP_CHANNEL, 0, "%s: number is used by another channel",
                               where);
            else
                numbers[seen++] = ch.number;
        }
        if (mode_ok) {
            if (ch.mode == CTW_MODE_TIMED) parse_timed(pc, t, &ch, where);
            else parse_looping(pc, t, &ch, where);
        }
        pc->channel = -1;
        if (pc->bad == bad) out[count++] = ch;
    }
    pc->db->channels = out;
    pc->db->channel_count = count;
}

static void sort_channels(ctw_db *db)
{
    size_t a, b;
    for (a = 1; a < db->channel_count; a++) {
        ctw_channel cur = db->channels[a];
        for (b = a; b > 0 && cur.number < db->channels[b - 1].number; b--)
            db->channels[b] = db->channels[b - 1];
        db->channels[b] = cur;
    }
}

int ctw_config_parse(ctw_db *db, const char *toml_text, ctw_issues *issues)
{
    toml_table_t *root;
    toml_array_t *arr;
    int64_t seed;
    ctw_pctx pc;
    ctw_pctx_init(&pc, db, issues, CFG_FILE, CTW_E_PARSE);
    db->channels = NULL;
    db->channel_count = 0;
    root = ctw_toml_open(&pc, toml_text);
    if (!root) return -1;
    ctw_check_version(&pc, root);
    if (ctw_get_int(&pc, root, "seed", 0, 4294967295LL, &seed, "config") == 0)
        db->seed = (uint32_t)seed;
    arr = toml_array_in(root, "channel");
    if (arr) parse_channels(&pc, arr);
    else if (toml_key_exists(root, "channel")) ctw_pctx_wrong_top_level(&pc, "channel");
    sort_channels(db);
    toml_free(root);
    return pc.bad ? -1 : 0;
}
