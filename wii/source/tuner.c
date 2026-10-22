#include "tuner.h"

#include <stdlib.h>
#include <string.h>

#define TN_NEXT_EPISODE_STEPS 64

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

static int64_t days_from_civil(ctw_date d)
{
    int64_t y = d.year - (d.month <= 2 ? 1 : 0);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t mp = d.month > 2 ? d.month - 3 : d.month + 9;
    int64_t doy = (153 * mp + 2) / 5 + d.day - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static bool in_lineup(const ctw_lineup *l, const ctw_airing *a)
{
    return l->items != NULL && a >= l->items && a < l->items + l->count;
}

const ctw_airing *tn_next(tuner *t, int channel, const ctw_airing *a)
{
    tuner_entry *e = find_entry(t, channel);
    if (e == NULL || !e->valid || a == NULL) return NULL;
    if (in_lineup(&e->today, a)) return ctw_lineup_next(&e->today, a);
    if (!in_lineup(&e->yesterday, a)) return NULL;
    const ctw_airing *n = ctw_lineup_next(&e->yesterday, a);
    if (n != NULL) return n;
    int64_t end = a->start_ms + (int64_t)a->duration_ms - (int64_t)CTW_DAY_MS;
    if (end < 0 || end >= (int64_t)CTW_DAY_MS) return NULL;
    return ctw_lineup_at(&e->today, &e->yesterday, (uint32_t)end, NULL);
}

const ctw_airing *tn_next_episode(tuner *t, int channel, const ctw_airing *a)
{
    const ctw_airing *n = a;
    for (int i = 0; i < TN_NEXT_EPISODE_STEPS && n != NULL; i++) {
        n = tn_next(t, channel, n);
        if (n != NULL && n->kind == CTW_AIR_EPISODE) return n;
    }
    return NULL;
}

int64_t tn_ms_until(const tuner *t, int channel, const ctw_airing *from, uint32_t offset_ms, const ctw_airing *to)
{
    int64_t a = tn_airing_start(t, channel, from);
    int64_t b = tn_airing_start(t, channel, to);
    if (a < 0 || b < 0) return 0;
    int64_t d = b - (a + (int64_t)offset_ms);
    return d < 0 ? 0 : d;
}

int64_t tn_airing_start(const tuner *t, int channel, const ctw_airing *a)
{
    for (size_t i = 0; i < t->count; i++) {
        const tuner_entry *e = &t->entries[i];
        if (e->channel != channel || !e->valid || a == NULL) continue;
        int64_t day = days_from_civil(e->day);
        if (in_lineup(&e->yesterday, a)) day -= 1;
        else if (!in_lineup(&e->today, a)) return -1;
        return day * (int64_t)CTW_DAY_MS + a->start_ms;
    }
    return -1;
}
