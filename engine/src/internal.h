#ifndef CTW_INTERNAL_H
#define CTW_INTERNAL_H

#include <stddef.h>
#include "ctw/ctw.h"

typedef struct ctw_chunk ctw_chunk;

typedef struct { ctw_chunk *head; } ctw_arena;

struct ctw_db {
    ctw_arena arena;
    ctw_show *shows;
    size_t show_count;
    ctw_ad *ads;
    size_t ad_count;
};

void ctw_arena_init(ctw_arena *a);
void ctw_arena_free(ctw_arena *a);
void *ctw_arena_alloc(ctw_arena *a, size_t n);
char *ctw_arena_strdup(ctw_arena *a, const char *s);

#endif
