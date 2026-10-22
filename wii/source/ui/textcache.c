#include "textcache.h"

#include <stdlib.h>
#include <string.h>

static uint32_t hash_text(const char *s)
{
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

static void release(tc_cache *c, tc_entry *e)
{
    if (!e->used) return;
    if (c->free_payload != NULL && e->payload != NULL) c->free_payload(e->payload);
    free(e->text);
    memset(e, 0, sizeof *e);
}

void tc_init(tc_cache *c, tc_free_fn free_payload)
{
    tc_init_capacity(c, free_payload, TC_CAPACITY);
}

void tc_init_capacity(tc_cache *c, tc_free_fn free_payload, size_t capacity)
{
    memset(c, 0, sizeof *c);
    c->free_payload = free_payload;
    if (capacity < 1) capacity = 1;
    if (capacity > TC_CAPACITY) capacity = TC_CAPACITY;
    c->capacity = capacity;
}

tc_entry *tc_find(tc_cache *c, int font, int px, uint32_t rgba, const char *text)
{
    uint32_t h = hash_text(text);
    for (size_t i = 0; i < c->capacity; i++) {
        tc_entry *e = &c->entries[i];
        if (!e->used || e->hash != h || e->font != font || e->px != px || e->rgba != rgba) continue;
        if (strcmp(e->text, text) != 0) continue;
        e->last_use = ++c->tick;
        return e;
    }
    return NULL;
}

tc_entry *tc_insert(tc_cache *c, int font, int px, uint32_t rgba, const char *text, void *payload)
{
    size_t len = strlen(text);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, text, len + 1);

    tc_entry *slot = NULL;
    for (size_t i = 0; i < c->capacity; i++) {
        tc_entry *e = &c->entries[i];
        if (!e->used) {
            slot = e;
            break;
        }
        if (slot == NULL || e->last_use < slot->last_use) slot = e;
    }
    release(c, slot);

    slot->used = true;
    slot->font = font;
    slot->px = px;
    slot->rgba = rgba;
    slot->hash = hash_text(text);
    slot->text = copy;
    slot->last_use = ++c->tick;
    slot->payload = payload;
    return slot;
}

void tc_clear(tc_cache *c)
{
    for (size_t i = 0; i < c->capacity; i++) release(c, &c->entries[i]);
    c->tick = 0;
}

bool tc_cacheable(bool needs_texture, bool has_texture)
{
    return !needs_texture || has_texture;
}
