#ifndef CTW_UI_TEXTCACHE_H
#define CTW_UI_TEXTCACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TC_CAPACITY 256

typedef void (*tc_free_fn)(void *payload);

typedef struct {
    bool used;
    int font;
    int px;
    uint32_t rgba;
    uint32_t hash;
    char *text;
    uint32_t last_use;
    void *payload;
} tc_entry;

typedef struct {
    tc_entry entries[TC_CAPACITY];
    size_t capacity;
    uint32_t tick;
    tc_free_fn free_payload;
} tc_cache;

void tc_init(tc_cache *c, tc_free_fn free_payload);
void tc_init_capacity(tc_cache *c, tc_free_fn free_payload, size_t capacity);
tc_entry *tc_find(tc_cache *c, int font, int px, uint32_t rgba, const char *text);
tc_entry *tc_insert(tc_cache *c, int font, int px, uint32_t rgba, const char *text, void *payload);
void tc_clear(tc_cache *c);

#endif
