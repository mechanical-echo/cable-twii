#ifndef CTW_H
#define CTW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CTW_FNV_OFFSET 0xcbf29ce484222325ULL
#define CTW_RNG_STREAM 0x63747721ULL
#define CTW_DAY_MS 86400000u

typedef struct { int year; int month; int day; } ctw_date;

typedef struct { uint64_t state; uint64_t inc; } ctw_rng;

typedef struct {
    const char *path;
    const char *title;
    const char *description;
    uint16_t season;
    uint16_t episode;
    uint32_t duration_ms;
} ctw_episode;

typedef struct {
    const char *channel_folder;
    const char *folder;
    const char *name;
    const char *icon;
    ctw_episode *episodes;
    size_t episode_count;
} ctw_show;

typedef struct {
    const char *path;
    const char *channel_folder;
    uint32_t duration_ms;
} ctw_ad;

typedef enum {
    CTW_ITEM_EPISODE, CTW_ITEM_RANDOM, CTW_ITEM_RANDOM_RUN,
    CTW_ITEM_ADS, CTW_ITEM_AD, CTW_ITEM_FILL
} ctw_item_type;

typedef struct {
    ctw_item_type type;
    const ctw_show *show;
    uint16_t season;
    uint16_t episode;
    uint16_t count;
    uint16_t ads_between;
    const ctw_ad *ad;
} ctw_item;

typedef struct { uint32_t at_ms; ctw_item *items; size_t item_count; } ctw_slot;

typedef enum { CTW_MODE_TIMED, CTW_MODE_LOOPING } ctw_mode;

typedef struct {
    int number;
    const char *name;
    const char *category;
    const char *folder;
    const char *icon;
    bool ads;
    ctw_mode mode;
    ctw_slot *slots;
    size_t slot_count;
    ctw_item *loop;
    size_t loop_count;
} ctw_channel;

typedef struct ctw_db ctw_db;

typedef enum { CTW_LEVEL_ERROR, CTW_LEVEL_WARNING } ctw_level;

typedef enum {
    CTW_E_PARSE, CTW_E_VERSION, CTW_E_BAD_VALUE, CTW_E_BAD_TIME,
    CTW_E_DUP_CHANNEL, CTW_E_UNKNOWN_SHOW, CTW_E_MISSING_EPISODE,
    CTW_E_UNKNOWN_AD, CTW_E_FILL_IN_LOOP, CTW_E_NO_SLOTS, CTW_E_DUP_SLOT,
    CTW_E_MISSING_FILE, CTW_E_EMPTY_POOL,
    CTW_W_CUT, CTW_W_PARTIAL, CTW_W_LOOP_EMPTY
} ctw_code;

typedef struct {
    ctw_level level;
    ctw_code code;
    int channel;
    int slot;
    int item;
    int line;
    char file[16];
    char message[200];
} ctw_issue;

typedef struct { ctw_issue *items; size_t count; size_t cap; } ctw_issues;

typedef enum { CTW_AIR_EPISODE, CTW_AIR_AD, CTW_AIR_UPNEXT } ctw_air_kind;

typedef struct {
    int64_t start_ms;
    uint32_t duration_ms;
    ctw_air_kind kind;
    const ctw_show *show;
    const ctw_episode *episode;
    const ctw_ad *ad;
    int16_t slot_index;
    int16_t item_index;
} ctw_airing;

typedef struct { ctw_airing *items; size_t count; size_t cap; ctw_issues warnings; } ctw_lineup;

ctw_rng ctw_rng_seed(uint64_t initstate, uint64_t initseq);
uint32_t ctw_rng_next(ctw_rng *r);
uint32_t ctw_rng_below(ctw_rng *r, uint32_t bound);
void ctw_shuffle(ctw_rng *r, uint32_t *items, size_t n);

uint64_t ctw_fnv1a(uint64_t h, const void *data, size_t len);
uint64_t ctw_day_seed(uint32_t seed, int channel_number, ctw_date day);

int ctw_parse_hhmm(const char *s, uint32_t *out_ms);
uint32_t ctw_date_key(ctw_date d);
ctw_date ctw_date_prev(ctw_date d);
ctw_date ctw_date_next(ctw_date d);
int ctw_date_parse(const char *s, ctw_date *out);

ctw_db *ctw_db_new(void);
void ctw_db_free(ctw_db *db);
int ctw_library_parse(ctw_db *db, const char *toml_text, ctw_issues *issues);
const ctw_show *ctw_db_show(const ctw_db *db, const char *channel_folder, const char *folder);
size_t ctw_db_show_count(const ctw_db *db);
const ctw_show *ctw_db_show_at(const ctw_db *db, size_t i);
size_t ctw_db_ad_count(const ctw_db *db);
const ctw_ad *ctw_db_ad_at(const ctw_db *db, size_t i);
int ctw_config_parse(ctw_db *db, const char *toml_text, ctw_issues *issues);
int ctw_db_load(const char *root, ctw_db **out, ctw_issues *issues);
int ctw_db_check_files(const ctw_db *db, const char *root, ctw_issues *issues);
uint32_t ctw_db_seed(const ctw_db *db);
size_t ctw_db_channel_count(const ctw_db *db);
const ctw_channel *ctw_db_channel_at(const ctw_db *db, size_t i);
const ctw_channel *ctw_db_channel(const ctw_db *db, int number);

void ctw_issues_add(ctw_issues *l, ctw_level lv, ctw_code c, int channel, int slot, int item,
                    int line, const char *file, const char *fmt, ...);
bool ctw_issues_has_errors(const ctw_issues *l);
void ctw_issues_free(ctw_issues *l);

int ctw_lineup_build(const ctw_db *db, const ctw_channel *ch, ctw_date day, ctw_lineup *out);
void ctw_lineup_free(ctw_lineup *l);
uint64_t ctw_lineup_hash(const ctw_lineup *l);
const ctw_airing *ctw_lineup_at(const ctw_lineup *today, const ctw_lineup *yesterday,
                                uint32_t ms_of_day, uint32_t *offset_ms);
const ctw_airing *ctw_lineup_next(const ctw_lineup *l, const ctw_airing *a);

#endif
