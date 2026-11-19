#ifndef CTW_TUNER_H
#define CTW_TUNER_H

#include <stddef.h>
#include <stdint.h>
#include "ctw/ctw.h"

typedef struct {
    int channel;
    ctw_date day;
    ctw_lineup today;
    ctw_lineup yesterday;
    bool valid;
    int64_t shift_ms;
} tuner_entry;

typedef struct {
    const ctw_db *db;
    tuner_entry *entries;
    size_t count;
} tuner;

void tn_init(tuner *t, const ctw_db *db);
void tn_free(tuner *t);
const ctw_airing *tn_current(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, uint32_t *offset_ms);
const ctw_airing *tn_next(tuner *t, int channel, const ctw_airing *a);
const ctw_airing *tn_next_episode(tuner *t, int channel, const ctw_airing *a);
int64_t tn_ms_until(const tuner *t, int channel, const ctw_airing *from, uint32_t offset_ms, const ctw_airing *to);
int64_t tn_airing_start(const tuner *t, int channel, const ctw_airing *a);
uint32_t tn_ms_remaining(const ctw_airing *a, uint32_t offset_ms);
int64_t tn_shift(const tuner *t, int channel);
const ctw_airing *tn_prev(tuner *t, int channel, const ctw_airing *a);
bool tn_skip(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, int dir);
bool tn_jump(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, const ctw_airing *to);
const ctw_lineup *tn_lineup_now(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, ctw_date *lineup_day);

#endif
