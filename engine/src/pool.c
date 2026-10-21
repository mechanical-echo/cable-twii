#include <stdlib.h>
#include <string.h>
#include "pool.h"

static bool in_pool(const ctw_episode *e, uint16_t season)
{
    return season == 0 || e->season == season;
}

size_t ctw_pool_list(const ctw_show *show, uint16_t season, const ctw_episode **out, size_t cap)
{
    size_t n = 0;
    for (size_t i = 0; i < show->episode_count && n < cap; i++) {
        if (in_pool(&show->episodes[i], season)) out[n++] = &show->episodes[i];
    }
    return n;
}

ctw_bag *ctw_bagset_get(ctw_bagset *s, const ctw_show *show, uint16_t season)
{
    ctw_bag *b;
    size_t n = 0;
    uint32_t *order = NULL;

    for (size_t i = 0; i < s->count; i++) {
        if (s->bags[i].show == show && s->bags[i].season == season) return &s->bags[i];
    }

    for (size_t i = 0; i < show->episode_count; i++) {
        if (in_pool(&show->episodes[i], season)) n++;
    }
    if (n) {
        order = malloc(n * sizeof *order);
        if (!order) return NULL;
        n = 0;
        for (size_t i = 0; i < show->episode_count; i++) {
            if (in_pool(&show->episodes[i], season)) order[n++] = (uint32_t)i;
        }
    }

    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        ctw_bag *grown = realloc(s->bags, cap * sizeof *grown);
        if (!grown) {
            free(order);
            return NULL;
        }
        s->bags = grown;
        s->cap = cap;
    }

    b = &s->bags[s->count++];
    b->show = show;
    b->season = season;
    b->order = order;
    b->n = n;
    b->pos = 0;
    b->shuffled = false;
    return b;
}

const ctw_episode *ctw_bag_draw_fitting(ctw_bag *b, ctw_rng *r, uint32_t max_ms)
{
    if (b->n == 0) return NULL;

    if (!b->shuffled) {
        ctw_shuffle(r, b->order, b->n);
        b->shuffled = true;
        b->pos = 0;
    } else if (b->pos == b->n) {
        ctw_shuffle(r, b->order, b->n);
        b->pos = 0;
    }

    for (size_t i = b->pos; i < b->n; i++) {
        const ctw_episode *e = &b->show->episodes[b->order[i]];
        if (max_ms == UINT32_MAX || e->duration_ms <= max_ms) {
            uint32_t tmp = b->order[b->pos];
            b->order[b->pos] = b->order[i];
            b->order[i] = tmp;
            b->pos++;
            return e;
        }
    }
    return NULL;
}

const ctw_ad *ctw_ads_draw_fitting(const ctw_db *db, const ctw_channel *ch, ctw_bagset *s,
                                   ctw_rng *r, uint32_t max_ms)
{
    size_t total = ctw_db_ad_count(db);
    const char *folder = "";
    size_t k = 0;
    size_t pick;
    bool exclude;

    for (size_t i = 0; i < total; i++) {
        if (strcmp(ctw_db_ad_at(db, i)->channel_folder, ch->folder) == 0) {
            folder = ch->folder;
            break;
        }
    }

    for (size_t i = 0; i < total; i++) {
        const ctw_ad *a = ctw_db_ad_at(db, i);
        if (strcmp(a->channel_folder, folder) == 0 && a->duration_ms <= max_ms) k++;
    }
    if (k == 0) return NULL;

    exclude = false;
    if (k > 1 && s->has_last_ad && s->last_ad < total) {
        const ctw_ad *last = ctw_db_ad_at(db, s->last_ad);
        exclude = strcmp(last->channel_folder, folder) == 0 && last->duration_ms <= max_ms;
    }

    pick = ctw_rng_below(r, (uint32_t)(exclude ? k - 1 : k));

    for (size_t i = 0; i < total; i++) {
        const ctw_ad *a = ctw_db_ad_at(db, i);
        if (strcmp(a->channel_folder, folder) != 0 || a->duration_ms > max_ms) continue;
        if (exclude && i == s->last_ad) continue;
        if (pick == 0) {
            s->last_ad = (uint32_t)i;
            s->has_last_ad = true;
            return a;
        }
        pick--;
    }
    return NULL;
}

void ctw_bagset_free(ctw_bagset *s)
{
    if (!s) return;
    for (size_t i = 0; i < s->count; i++) free(s->bags[i].order);
    free(s->bags);
    s->bags = NULL;
    s->count = 0;
    s->cap = 0;
    s->has_last_ad = false;
    s->last_ad = 0;
}
