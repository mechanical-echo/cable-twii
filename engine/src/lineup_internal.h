#ifndef CTW_LINEUP_INTERNAL_H
#define CTW_LINEUP_INTERNAL_H

#include <stdint.h>
#include "ctw/ctw.h"
#include "pool.h"

typedef struct {
    const ctw_db *db;
    const ctw_channel *ch;
    ctw_lineup *out;
    ctw_bagset bags;
    ctw_rng rng;
    int64_t cursor;
} ctw_build;

uint32_t ctw_build_left(const ctw_build *b, int64_t end);
int ctw_build_push(ctw_build *b, uint32_t duration_ms, ctw_air_kind kind, const ctw_show *show,
                   const ctw_episode *episode, const ctw_ad *ad, int slot, int item);
void ctw_build_warn(ctw_build *b, ctw_code code, int slot, int item, const char *message);
int ctw_build_fill_leftover(ctw_build *b, int64_t end);
int ctw_lineup_build_timed(ctw_build *b);

#endif
