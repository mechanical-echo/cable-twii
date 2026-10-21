#include <stdlib.h>
#include <string.h>
#include "internal.h"

#define CTW_ARENA_CHUNK 16384u
#define CTW_ARENA_ALIGN 8u

struct ctw_chunk {
    ctw_chunk *next;
    size_t used;
    size_t cap;
    unsigned char data[];
};

void ctw_arena_init(ctw_arena *a)
{
    a->head = NULL;
}

void ctw_arena_free(ctw_arena *a)
{
    ctw_chunk *c = a->head;
    while (c) {
        ctw_chunk *next = c->next;
        free(c);
        c = next;
    }
    a->head = NULL;
}

void *ctw_arena_alloc(ctw_arena *a, size_t n)
{
    ctw_chunk *c = a->head;
    size_t need = (n + (CTW_ARENA_ALIGN - 1)) & ~(size_t)(CTW_ARENA_ALIGN - 1);
    void *p;
    if (need < n) return NULL;
    if (need == 0) need = CTW_ARENA_ALIGN;
    if (!c || c->cap - c->used < need) {
        size_t cap = need > CTW_ARENA_CHUNK ? need : CTW_ARENA_CHUNK;
        ctw_chunk *fresh = malloc(sizeof *fresh + cap);
        if (!fresh) return NULL;
        fresh->used = 0;
        fresh->cap = cap;
        if (c && need > CTW_ARENA_CHUNK) {
            fresh->next = c->next;
            c->next = fresh;
            fresh->used = need;
            return fresh->data;
        }
        fresh->next = a->head;
        a->head = fresh;
        c = fresh;
    }
    p = c->data + c->used;
    c->used += need;
    return p;
}

char *ctw_arena_strdup(ctw_arena *a, const char *s)
{
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = ctw_arena_alloc(a, n);
    if (p) memcpy(p, s, n);
    return p;
}
