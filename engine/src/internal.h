#ifndef CTW_INTERNAL_H
#define CTW_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "ctw/ctw.h"
#include "tomlc99/toml.h"

typedef struct ctw_chunk ctw_chunk;

typedef struct { ctw_chunk *head; } ctw_arena;

struct ctw_db {
    ctw_arena arena;
    ctw_show *shows;
    size_t show_count;
    ctw_ad *ads;
    size_t ad_count;
    uint32_t seed;
    ctw_channel *channels;
    size_t channel_count;
};

typedef struct {
    ctw_db *db;
    ctw_issues *issues;
    const char *file;
    ctw_code type_code;
    int channel;
    int slot;
    int item;
    int bad;
} ctw_pctx;

void ctw_pctx_init(ctw_pctx *pc, ctw_db *db, ctw_issues *issues, const char *file,
                   ctw_code type_code);
void ctw_pctx_error(ctw_pctx *pc, ctw_code code, int line, const char *fmt, ...);
void ctw_pctx_oom(ctw_pctx *pc);
void ctw_pctx_not_table(ctw_pctx *pc, const char *where);
void ctw_pctx_wrong_top_level(ctw_pctx *pc, const char *key);
int ctw_get_int(ctw_pctx *pc, const toml_table_t *t, const char *key, int64_t lo, int64_t hi,
                int64_t *out, const char *where);
int ctw_get_str(ctw_pctx *pc, const toml_table_t *t, const char *key, int required,
                const char **out, const char *where);
int ctw_get_bool(ctw_pctx *pc, const toml_table_t *t, const char *key, int *out,
                 const char *where);
toml_table_t *ctw_toml_open(ctw_pctx *pc, const char *text);
void ctw_check_version(ctw_pctx *pc, const toml_table_t *root);

void ctw_arena_init(ctw_arena *a);
void ctw_arena_free(ctw_arena *a);
void *ctw_arena_alloc(ctw_arena *a, size_t n);
char *ctw_arena_strdup(ctw_arena *a, const char *s);

#endif
