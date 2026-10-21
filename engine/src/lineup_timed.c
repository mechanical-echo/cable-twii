#include <stdlib.h>
#include "lineup_internal.h"

static const char *const msg_cut = "does not fit the slot and is cut at the slot end";
static const char *const msg_partial = "could not place everything in the time left";

static const ctw_episode *find_episode(const ctw_show *show, uint16_t season, uint16_t episode)
{
    for (size_t i = 0; i < show->episode_count; i++) {
        const ctw_episode *e = &show->episodes[i];
        if (e->season == season && e->episode == episode) return e;
    }
    return NULL;
}

static void mark_last_ad(ctw_build *b, const ctw_ad *ad)
{
    size_t total = ctw_db_ad_count(b->db);
    for (size_t i = 0; i < total; i++) {
        if (ctw_db_ad_at(b->db, i) == ad) {
            b->bags.last_ad = (uint32_t)i;
            b->bags.has_last_ad = true;
            return;
        }
    }
}

static int place_fixed(ctw_build *b, int64_t end, ctw_air_kind kind, const ctw_show *show,
                       const ctw_episode *episode, const ctw_ad *ad, uint32_t duration_ms,
                       int slot, int item)
{
    uint32_t left = ctw_build_left(b, end);
    if (left == 0u) {
        ctw_build_warn(b, CTW_W_PARTIAL, slot, item, msg_partial);
        return 0;
    }
    if (duration_ms > left) {
        duration_ms = left;
        ctw_build_warn(b, CTW_W_CUT, slot, item, msg_cut);
    }
    if (ctw_build_push(b, duration_ms, kind, show, episode, ad, slot, item)) return -1;
    if (ad) mark_last_ad(b, ad);
    return 0;
}

static int place_random(ctw_build *b, const ctw_item *it, int64_t end, int slot, int item)
{
    ctw_bag *bag = ctw_bagset_get(&b->bags, it->show, it->season);
    if (!bag) return -1;
    for (uint32_t k = 0; k < it->count; k++) {
        const ctw_episode *e = ctw_bag_draw_fitting(bag, &b->rng, ctw_build_left(b, end));
        if (!e) {
            ctw_build_warn(b, CTW_W_PARTIAL, slot, item, msg_partial);
            break;
        }
        if (ctw_build_push(b, e->duration_ms, CTW_AIR_EPISODE, it->show, e, NULL, slot, item)) {
            return -1;
        }
    }
    return 0;
}

static int place_random_run(ctw_build *b, const ctw_item *it, int64_t end, int slot, int item)
{
    size_t n = ctw_pool_list(it->show, it->season, NULL, 0);
    size_t count = (size_t)it->count;
    const ctw_episode **list = NULL;
    size_t start = 0;
    size_t taken = 0;
    int partial = 0;
    int rc = 0;

    if (n) {
        list = malloc(n * sizeof *list);
        if (!list) return -1;
        ctw_pool_list(it->show, it->season, list, n);
    }
    if (n < count) {
        partial = 1;
    } else {
        start = (size_t)ctw_rng_below(&b->rng, (uint32_t)(n - count + 1u));
    }
    for (size_t k = start; k < n && taken < count; k++) {
        if (list[k]->duration_ms > ctw_build_left(b, end)) break;
        if (ctw_build_push(b, list[k]->duration_ms, CTW_AIR_EPISODE, it->show, list[k], NULL,
                           slot, item)) {
            rc = -1;
            break;
        }
        taken++;
    }
    if (rc == 0 && (partial || taken < count)) {
        ctw_build_warn(b, CTW_W_PARTIAL, slot, item, msg_partial);
    }
    free(list);
    return rc;
}

static int place_ads(ctw_build *b, uint32_t count, int64_t end, int slot, int item)
{
    for (uint32_t k = 0; k < count; k++) {
        const ctw_ad *a = ctw_ads_draw_fitting(b->db, b->ch, &b->bags, &b->rng,
                                               ctw_build_left(b, end));
        if (!a) break;
        if (ctw_build_push(b, a->duration_ms, CTW_AIR_AD, NULL, NULL, a, slot, item)) return -1;
    }
    return 0;
}

static int place_fill(ctw_build *b, const ctw_item *it, int64_t end, int slot, int item)
{
    ctw_bag *bag = ctw_bagset_get(&b->bags, it->show, it->season);
    if (!bag) return -1;
    for (;;) {
        const ctw_episode *e = ctw_bag_draw_fitting(bag, &b->rng, ctw_build_left(b, end));
        if (!e) break;
        if (ctw_build_push(b, e->duration_ms, CTW_AIR_EPISODE, it->show, e, NULL, slot, item)) {
            return -1;
        }
        if (b->ch->ads && place_ads(b, it->ads_between, end, slot, item)) return -1;
    }
    return 0;
}

static int place_item(ctw_build *b, const ctw_item *it, int64_t end, int slot, int item)
{
    const ctw_episode *e;
    switch (it->type) {
    case CTW_ITEM_EPISODE:
        if (!it->show) return -1;
        e = find_episode(it->show, it->season, it->episode);
        if (!e) return -1;
        return place_fixed(b, end, CTW_AIR_EPISODE, it->show, e, NULL, e->duration_ms, slot, item);
    case CTW_ITEM_AD:
        if (!it->ad) return -1;
        return place_fixed(b, end, CTW_AIR_AD, NULL, NULL, it->ad, it->ad->duration_ms, slot, item);
    case CTW_ITEM_RANDOM:
        if (!it->show) return -1;
        return place_random(b, it, end, slot, item);
    case CTW_ITEM_RANDOM_RUN:
        if (!it->show) return -1;
        return place_random_run(b, it, end, slot, item);
    case CTW_ITEM_FILL:
        if (!it->show) return -1;
        return place_fill(b, it, end, slot, item);
    case CTW_ITEM_ADS:
        if (!b->ch->ads) return 0;
        return place_ads(b, it->count, end, slot, item);
    }
    return -1;
}

int ctw_lineup_build_timed(ctw_build *b)
{
    const ctw_channel *ch = b->ch;
    if (ch->slot_count == 0u || !ch->slots) return -1;
    for (size_t i = 0; i < ch->slot_count; i++) {
        const ctw_slot *sl = &ch->slots[i];
        int64_t end;
        if (i + 1u < ch->slot_count) {
            end = (int64_t)ch->slots[i + 1u].at_ms;
        } else {
            end = (int64_t)ch->slots[0].at_ms + (int64_t)CTW_DAY_MS;
        }
        b->cursor = (int64_t)sl->at_ms;
        for (size_t j = 0; j < sl->item_count; j++) {
            if (place_item(b, &sl->items[j], end, (int)i, (int)j)) return -1;
        }
        if (ctw_build_fill_leftover(b, end)) return -1;
    }
    return 0;
}
