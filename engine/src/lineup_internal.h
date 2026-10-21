#ifndef CTW_LINEUP_INTERNAL_H
#define CTW_LINEUP_INTERNAL_H

#include <stdint.h>
#include "ctw/ctw.h"
#include "pool.h"

#define CTW_LINEUP_MAX_AIRINGS 20000u

typedef struct {
    const ctw_db *db;
    const ctw_channel *ch;
    ctw_lineup *out;
    ctw_bagset bags;
    ctw_rng rng;
    int64_t cursor;
    int64_t limit;
    int64_t cover_end;
    int capped;
} ctw_build;

uint32_t ctw_build_left(const ctw_build *b, int64_t end);
int ctw_build_push(ctw_build *b, uint32_t duration_ms, ctw_air_kind kind, const ctw_show *show,
                   const ctw_episode *episode, const ctw_ad *ad, int slot, int item);
void ctw_build_warn(ctw_build *b, ctw_code code, int slot, int item, const char *message);
int ctw_build_fill_leftover(ctw_build *b, int64_t end);
int ctw_place_item(ctw_build *b, const ctw_item *it, int64_t end, int slot, int item);
int ctw_lineup_build_timed(ctw_build *b);
int ctw_lineup_build_loop(ctw_build *b);

#endif
