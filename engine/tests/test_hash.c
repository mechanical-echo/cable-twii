#include "greatest/greatest.h"
#include "ctw/ctw.h"

TEST fnv_known_values(void)
{
    ASSERT_EQ(0xcbf29ce484222325ULL, ctw_fnv1a(CTW_FNV_OFFSET, "", 0));
    ASSERT_EQ(0xaf63dc4c8601ec8cULL, ctw_fnv1a(CTW_FNV_OFFSET, "a", 1));
    PASS();
}

TEST day_seed_varies(void)
{
    ctw_date d = {2026, 10, 20}, e = {2026, 10, 21};
    ASSERT(ctw_day_seed(1, 1, d) != ctw_day_seed(1, 1, e));
    ASSERT(ctw_day_seed(1, 1, d) != ctw_day_seed(1, 2, d));
    ASSERT(ctw_day_seed(1, 1, d) != ctw_day_seed(2, 1, d));
    ASSERT_EQ(ctw_day_seed(1, 1, d), ctw_day_seed(1, 1, d));
    PASS();
}

TEST day_seed_matches_big_endian_bytes(void)
{
    ctw_date d = {2026, 10, 20};
    uint32_t key = 20261020u;
    unsigned char bytes[12] = {
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x02,
        (unsigned char)(key >> 24), (unsigned char)(key >> 16),
        (unsigned char)(key >> 8), (unsigned char)key
    };
    ASSERT_EQ(ctw_fnv1a(CTW_FNV_OFFSET, bytes, 12), ctw_day_seed(1, 2, d));
    PASS();
}

SUITE(hash_suite)
{
    RUN_TEST(fnv_known_values);
    RUN_TEST(day_seed_varies);
    RUN_TEST(day_seed_matches_big_endian_bytes);
}
