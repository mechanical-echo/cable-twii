#include <stdio.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ui/textcache.h"

static int freed[TC_CAPACITY + 8];
static size_t freed_count;

static void record_free(void *payload)
{
    freed[freed_count++] = (int)(size_t)payload;
}

static void reset_freed(void)
{
    freed_count = 0;
}

static tc_cache cache;

TEST cache_miss_then_hit(void)
{
    reset_freed();
    tc_init(&cache, record_free);
    ASSERT(tc_find(&cache, 0, 15, 0x444444FFu, "Gumball") == NULL);
    ASSERT(tc_insert(&cache, 0, 15, 0x444444FFu, "Gumball", (void *)(size_t)7) != NULL);
    tc_entry *e = tc_find(&cache, 0, 15, 0x444444FFu, "Gumball");
    ASSERT(e != NULL);
    ASSERT_EQ(7, (int)(size_t)e->payload);
    tc_clear(&cache);
    ASSERT_EQ(1u, freed_count);
    PASS();
}

TEST cache_key_includes_font_size_colour(void)
{
    reset_freed();
    tc_init(&cache, record_free);
    tc_insert(&cache, 0, 15, 0x444444FFu, "Gumball", (void *)(size_t)1);
    ASSERT(tc_find(&cache, 1, 15, 0x444444FFu, "Gumball") == NULL);
    ASSERT(tc_find(&cache, 0, 16, 0x444444FFu, "Gumball") == NULL);
    ASSERT(tc_find(&cache, 0, 15, 0x999999FFu, "Gumball") == NULL);
    ASSERT(tc_find(&cache, 0, 15, 0x444444FFu, "Gumbal") == NULL);
    ASSERT(tc_find(&cache, 0, 15, 0x444444FFu, "Gumball") != NULL);
    tc_clear(&cache);
    PASS();
}

TEST cache_evicts_least_recently_used(void)
{
    reset_freed();
    tc_init(&cache, record_free);
    char key[16];
    for (int i = 0; i < TC_CAPACITY; i++) {
        snprintf(key, sizeof key, "s%d", i);
        ASSERT(tc_insert(&cache, 0, 15, 0xFFu, key, (void *)(size_t)(i + 1)) != NULL);
    }
    ASSERT_EQ(0u, freed_count);
    ASSERT(tc_find(&cache, 0, 15, 0xFFu, "s0") != NULL);
    ASSERT(tc_insert(&cache, 0, 15, 0xFFu, "new", (void *)(size_t)1000) != NULL);
    ASSERT_EQ(1u, freed_count);
    ASSERT_EQ(2, freed[0]);
    ASSERT(tc_find(&cache, 0, 15, 0xFFu, "s1") == NULL);
    ASSERT(tc_find(&cache, 0, 15, 0xFFu, "s0") != NULL);
    ASSERT(tc_find(&cache, 0, 15, 0xFFu, "new") != NULL);
    tc_clear(&cache);
    ASSERT_EQ((size_t)TC_CAPACITY + 1u, freed_count);
    PASS();
}

TEST cache_capacity_is_256(void)
{
    ASSERT_EQ(256, TC_CAPACITY);
    PASS();
}

SUITE(textcache_suite)
{
    RUN_TEST(cache_miss_then_hit);
    RUN_TEST(cache_key_includes_font_size_colour);
    RUN_TEST(cache_evicts_least_recently_used);
    RUN_TEST(cache_capacity_is_256);
}
