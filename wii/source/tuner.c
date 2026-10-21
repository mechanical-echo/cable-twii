#include "tuner.h"

#include <stdlib.h>
#include <string.h>

static tuner_entry *find_entry(tuner *t, int channel)
{
    for (size_t i = 0; i < t->count; i++) {
        if (t->entries[i].channel == channel) return &t->entries[i];
    }
    return NULL;
}

static tuner_entry *ensure_entry(tuner *t, int channel)
{
    tuner_entry *e = find_entry(t, channel);
    if (e != NULL) return e;
    tuner_entry *grown = (tuner_entry *)realloc(t->entries, (t->count + 1) * sizeof(tuner_entry));
    if (grown == NULL) return NULL;
    t->entries = grown;
    e = &t->entries[t->count++];
    memset(e, 0, sizeof(*e));
    e->channel = channel;
    e->valid = false;
    return e;
}

void tn_init(tuner *t, const ctw_db *db)
{
    t->db = db;
    t->entries = NULL;
    t->count = 0;
}

void tn_free(tuner *t)
{
    for (size_t i = 0; i < t->count; i++) {
        if (t->entries[i].valid) {
            ctw_lineup_free(&t->entries[i].today);
            ctw_lineup_free(&t->entries[i].yesterday);
        }
    }
    free(t->entries);
    t->entries = NULL;
    t->count = 0;
}

const ctw_airing *tn_current(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, uint32_t *offset_ms)
{
    tuner_entry *e = ensure_entry(t, channel);
    if (e == NULL) return NULL;

    if (!e->valid || ctw_date_key(e->day) != ctw_date_key(day)) {
        if (e->valid) {
            ctw_lineup_free(&e->today);
            ctw_lineup_free(&e->yesterday);
        }
        const ctw_channel *ch = ctw_db_channel(t->db, channel);
        if (ch == NULL) {
            e->valid = false;
            return NULL;
        }
        ctw_date prev = ctw_date_prev(day);
        memset(&e->today, 0, sizeof(e->today));
        memset(&e->yesterday, 0, sizeof(e->yesterday));
        ctw_lineup_build(t->db, ch, day, &e->today);
        ctw_lineup_build(t->db, ch, prev, &e->yesterday);
        e->day = day;
        e->valid = true;
    }

    return ctw_lineup_at(&e->today, &e->yesterday, ms_of_day, offset_ms);
}

uint32_t tn_ms_remaining(const ctw_airing *a, uint32_t offset_ms)
{
    if (a->duration_ms <= offset_ms) return 0;
    return a->duration_ms - offset_ms;
}
