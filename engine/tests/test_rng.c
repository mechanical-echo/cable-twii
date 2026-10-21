#include <stdlib.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"

TEST pcg32_reference_sequence(void)
{
    ctw_rng r = ctw_rng_seed(42u, 54u);
    uint32_t expected[6] = {0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e};
    for (int i = 0; i < 6; i++) {
        ASSERT_EQ_FMT(expected[i], ctw_rng_next(&r), "%08x");
    }
    PASS();
}

TEST bounded_stays_below(void)
{
    ctw_rng r = ctw_rng_seed(1, 2);
    for (int i = 0; i < 10000; i++) {
        ASSERT(ctw_rng_below(&r, 7) < 7);
    }
    PASS();
}

TEST bounded_zero_returns_zero(void)
{
    ctw_rng r = ctw_rng_seed(1, 2);
    ASSERT_EQ(0u, ctw_rng_below(&r, 0));
    PASS();
}

TEST bounded_one_returns_zero(void)
{
    ctw_rng r = ctw_rng_seed(1, 2);
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(0u, ctw_rng_below(&r, 1));
    }
    PASS();
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

TEST shuffle_is_permutation(void)
{
    uint32_t items[100];
    ctw_rng r = ctw_rng_seed(7, 9);
    for (uint32_t i = 0; i < 100; i++) {
        items[i] = i;
    }
    ctw_shuffle(&r, items, 100);
    qsort(items, 100, sizeof items[0], cmp_u32);
    for (uint32_t i = 0; i < 100; i++) {
        ASSERT_EQ(i, items[i]);
    }
    PASS();
}

TEST shuffle_is_deterministic(void)
{
    uint32_t a[50], b[50];
    ctw_rng ra = ctw_rng_seed(11, 13);
    ctw_rng rb = ctw_rng_seed(11, 13);
    for (uint32_t i = 0; i < 50; i++) {
        a[i] = i;
        b[i] = i;
    }
    ctw_shuffle(&ra, a, 50);
    ctw_shuffle(&rb, b, 50);
    for (int i = 0; i < 50; i++) {
        ASSERT_EQ(a[i], b[i]);
    }
    PASS();
}

TEST shuffle_handles_tiny_arrays(void)
{
    uint32_t one[1] = {5};
    ctw_rng r = ctw_rng_seed(1, 1);
    ctw_shuffle(&r, one, 1);
    ctw_shuffle(&r, one, 0);
    ASSERT_EQ(5u, one[0]);
    PASS();
}

SUITE(rng_suite)
{
    RUN_TEST(pcg32_reference_sequence);
    RUN_TEST(bounded_stays_below);
    RUN_TEST(bounded_zero_returns_zero);
    RUN_TEST(bounded_one_returns_zero);
    RUN_TEST(shuffle_is_permutation);
    RUN_TEST(shuffle_is_deterministic);
    RUN_TEST(shuffle_handles_tiny_arrays);
}
