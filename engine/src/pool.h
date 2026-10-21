#ifndef CTW_POOL_H
#define CTW_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ctw/ctw.h"

typedef struct {
    const ctw_show *show;
    uint16_t season;
    uint32_t *order;
    size_t n;
    size_t pos;
    bool shuffled;
} ctw_bag;

typedef struct {
    ctw_bag *bags;
    size_t count;
    size_t cap;
    uint32_t last_ad;
    bool has_last_ad;
} ctw_bagset;

ctw_bag *ctw_bagset_get(ctw_bagset *s, const ctw_show *show, uint16_t season);
const ctw_episode *ctw_bag_draw_fitting(ctw_bag *b, ctw_rng *r, uint32_t max_ms);
size_t ctw_pool_list(const ctw_show *show, uint16_t season, const ctw_episode **out, size_t cap);
const ctw_ad *ctw_ads_draw_fitting(const ctw_db *db, const ctw_channel *ch, ctw_bagset *s,
                                   ctw_rng *r, uint32_t max_ms);
void ctw_bagset_free(ctw_bagset *s);

#endif
