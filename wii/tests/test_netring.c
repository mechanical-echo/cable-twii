#include <string.h>
#include "greatest/greatest.h"
#include "media/netring.h"

#define SMALL 16u
#define BIG (NR_KEEP_BEHIND + 4096u)

static uint8_t small_mem[SMALL];
static uint8_t big_mem[BIG];
static uint8_t data[BIG];
static uint8_t out[BIG];

static void fill_data(void)
{
    for (size_t i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 7 + 1);
}

TEST append_and_copy_across_wrap(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, small_mem, SMALL);
    nr_append(&r, data, 12);
    nr_drop_before(&r, 8 + NR_KEEP_BEHIND);
    ASSERT_EQ((uint64_t)8, r.start);
    ASSERT_EQ((size_t)12, nr_space(&r));
    nr_append(&r, data + 12, 10);
    ASSERT_EQ((uint64_t)22, nr_end(&r));
    memset(out, 0, sizeof out);
    ASSERT_EQ((size_t)14, nr_copy(&r, 8, out, 32));
    ASSERT_MEM_EQ(data + 8, out, 14);
    PASS();
}

TEST contains_window_edges(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, small_mem, SMALL);
    nr_reset(&r, 100);
    nr_append(&r, data, 5);
    ASSERT(nr_contains(&r, r.start));
    ASSERT(nr_contains(&r, 104));
    ASSERT_FALSE(nr_contains(&r, nr_end(&r)));
    ASSERT_FALSE(nr_contains(&r, 99));
    PASS();
}

TEST drop_keeps_behind(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, big_mem, BIG);
    nr_append(&r, data, NR_KEEP_BEHIND + 100);
    nr_drop_before(&r, r.start + NR_KEEP_BEHIND + 10);
    ASSERT_EQ((uint64_t)10, r.start);
    ASSERT_EQ((size_t)(NR_KEEP_BEHIND + 90), r.len);
    ASSERT_EQ((size_t)4, nr_copy(&r, 10, out, 4));
    ASSERT_MEM_EQ(data + 10, out, 4);
    PASS();
}

TEST drop_inside_keep_behind_keeps_everything(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, big_mem, BIG);
    nr_append(&r, data, 1000);
    nr_drop_before(&r, NR_KEEP_BEHIND);
    ASSERT_EQ((uint64_t)0, r.start);
    ASSERT_EQ((size_t)1000, r.len);
    PASS();
}

TEST drop_past_end_empties_at_end(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, small_mem, SMALL);
    nr_append(&r, data, 6);
    nr_drop_before(&r, 6 + NR_KEEP_BEHIND + 50);
    ASSERT_EQ((uint64_t)6, r.start);
    ASSERT_EQ((size_t)0, r.len);
    PASS();
}

TEST reset_empties_at_pos(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, small_mem, SMALL);
    nr_append(&r, data, 9);
    nr_reset(&r, 5000);
    ASSERT_EQ((uint64_t)5000, r.start);
    ASSERT_EQ((uint64_t)5000, nr_end(&r));
    ASSERT_EQ((size_t)SMALL, nr_space(&r));
    ASSERT_FALSE(nr_contains(&r, 5000));
    ASSERT_EQ((size_t)0, nr_copy(&r, 5000, out, 4));
    PASS();
}

TEST copy_outside_window_is_empty(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, small_mem, SMALL);
    nr_reset(&r, 40);
    nr_append(&r, data, 8);
    ASSERT_EQ((size_t)0, nr_copy(&r, 39, out, 4));
    ASSERT_EQ((size_t)0, nr_copy(&r, 48, out, 4));
    ASSERT_EQ((size_t)3, nr_copy(&r, 45, out, 4));
    ASSERT_MEM_EQ(data + 5, out, 3);
    PASS();
}

TEST append_past_capacity_slides_window(void)
{
    fill_data();
    nr_ring r;
    nr_init(&r, small_mem, SMALL);
    nr_append(&r, data, 10);
    nr_append(&r, data + 10, 10);
    ASSERT_EQ((uint64_t)4, r.start);
    ASSERT_EQ((size_t)SMALL, r.len);
    ASSERT_EQ((size_t)SMALL, nr_copy(&r, 4, out, SMALL));
    ASSERT_MEM_EQ(data + 4, out, SMALL);
    nr_append(&r, data + 20, 40);
    ASSERT_EQ((uint64_t)44, r.start);
    ASSERT_EQ((uint64_t)60, nr_end(&r));
    ASSERT_EQ((size_t)SMALL, nr_copy(&r, 44, out, SMALL));
    ASSERT_MEM_EQ(data + 44, out, SMALL);
    PASS();
}

SUITE(netring_suite)
{
    RUN_TEST(append_and_copy_across_wrap);
    RUN_TEST(contains_window_edges);
    RUN_TEST(drop_keeps_behind);
    RUN_TEST(drop_inside_keep_behind_keeps_everything);
    RUN_TEST(drop_past_end_empties_at_end);
    RUN_TEST(reset_empties_at_pos);
    RUN_TEST(copy_outside_window_is_empty);
    RUN_TEST(append_past_capacity_slides_window);
}
