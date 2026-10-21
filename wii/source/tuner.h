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
} tuner_entry;

typedef struct {
    const ctw_db *db;
    tuner_entry *entries;
    size_t count;
} tuner;

void tn_init(tuner *t, const ctw_db *db);
void tn_free(tuner *t);
const ctw_airing *tn_current(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, uint32_t *offset_ms);
uint32_t tn_ms_remaining(const ctw_airing *a, uint32_t offset_ms);

#endif
