#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lineup_internal.h"

#define CTW_LINEUP_INITIAL_CAP 32u

static int16_t index16(int v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < -1) return -1;
    return (int16_t)v;
}

uint32_t ctw_build_left(const ctw_build *b, int64_t end)
{
    if (b->cursor >= end) return 0u;
    return (uint32_t)(end - b->cursor);
}

int ctw_build_push(ctw_build *b, uint32_t duration_ms, ctw_air_kind kind, const ctw_show *show,
                   const ctw_episode *episode, const ctw_ad *ad, int slot, int item)
{
    ctw_lineup *l = b->out;
    ctw_airing *a;
    if (l->count == l->cap) {
        size_t cap = l->cap ? l->cap * 2u : CTW_LINEUP_INITIAL_CAP;
        ctw_airing *grown = realloc(l->items, cap * sizeof *grown);
        if (!grown) return -1;
        l->items = grown;
        l->cap = cap;
    }
    a = &l->items[l->count++];
    a->start_ms = b->cursor;
    a->duration_ms = duration_ms;
    a->kind = kind;
    a->show = show;
    a->episode = episode;
    a->ad = ad;
    a->slot_index = index16(slot);
    a->item_index = index16(item);
    b->cursor += (int64_t)duration_ms;
    return 0;
}

void ctw_build_warn(ctw_build *b, ctw_code code, int slot, int item, const char *message)
{
    ctw_issues_add(&b->out->warnings, CTW_LEVEL_WARNING, code, b->ch->number, slot, item, 0,
                   "lineup", "%s", message);
}

int ctw_build_fill_leftover(ctw_build *b, int64_t end)
{
    if (b->ch->ads) {
        for (;;) {
            const ctw_ad *a = ctw_ads_draw_fitting(b->db, b->ch, &b->bags, &b->rng,
                                                   ctw_build_left(b, end));
            if (!a) break;
            if (ctw_build_push(b, a->duration_ms, CTW_AIR_AD, NULL, NULL, a, -1, -1)) return -1;
        }
    }
    if (b->cursor < end) {
        return ctw_build_push(b, ctw_build_left(b, end), CTW_AIR_UPNEXT, NULL, NULL, NULL, -1, -1);
    }
    return 0;
}

int ctw_lineup_build(const ctw_db *db, const ctw_channel *ch, ctw_date day, ctw_lineup *out)
{
    ctw_build b;
    int rc;
    if (!db || !ch || !out) return -1;
    memset(out, 0, sizeof *out);
    memset(&b, 0, sizeof b);
    b.db = db;
    b.ch = ch;
    b.out = out;
    b.rng = ctw_rng_seed(ctw_day_seed(ctw_db_seed(db), ch->number, day), CTW_RNG_STREAM);
    switch (ch->mode) {
    case CTW_MODE_TIMED:
        rc = ctw_lineup_build_timed(&b);
        break;
    case CTW_MODE_LOOPING:
    default:
        rc = -1;
        break;
    }
    ctw_bagset_free(&b.bags);
    if (rc) ctw_lineup_free(out);
    return rc;
}

void ctw_lineup_free(ctw_lineup *l)
{
    if (!l) return;
    free(l->items);
    l->items = NULL;
    l->count = 0;
    l->cap = 0;
    ctw_issues_free(&l->warnings);
}

static unsigned char kind_byte(ctw_air_kind kind)
{
    switch (kind) {
    case CTW_AIR_EPISODE:
        return 0u;
    case CTW_AIR_AD:
        return 1u;
    case CTW_AIR_UPNEXT:
        return 2u;
    }
    return 2u;
}

static const char *airing_path(const ctw_airing *a)
{
    if (a->kind == CTW_AIR_EPISODE && a->episode) return a->episode->path;
    if (a->kind == CTW_AIR_AD && a->ad) return a->ad->path;
    return NULL;
}

uint64_t ctw_lineup_hash(const ctw_lineup *l)
{
    uint64_t h = CTW_FNV_OFFSET;
    if (!l) return h;
    for (size_t i = 0; i < l->count; i++) {
        const ctw_airing *a = &l->items[i];
        uint64_t start = (uint64_t)a->start_ms;
        uint32_t dur = a->duration_ms;
        unsigned char buf[13];
        const char *path;
        for (int k = 0; k < 8; k++) buf[k] = (unsigned char)(start >> (56 - 8 * k));
        buf[8] = (unsigned char)(dur >> 24);
        buf[9] = (unsigned char)(dur >> 16);
        buf[10] = (unsigned char)(dur >> 8);
        buf[11] = (unsigned char)dur;
        buf[12] = kind_byte(a->kind);
        h = ctw_fnv1a(h, buf, sizeof buf);
        if (a->kind == CTW_AIR_UPNEXT) continue;
        path = airing_path(a);
        if (path) h = ctw_fnv1a(h, path, strlen(path));
        h = ctw_fnv1a(h, "", 1);
    }
    return h;
}
