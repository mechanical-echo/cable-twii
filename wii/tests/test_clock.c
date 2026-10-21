#include "greatest/greatest.h"
#include "clock.h"

TEST clock_epoch(void)
{
    ctw_date d;
    uint32_t ms;
    clk_split(0, &d, &ms);
    ASSERT_EQ(1970, d.year);
    ASSERT_EQ(1, d.month);
    ASSERT_EQ(1, d.day);
    ASSERT_EQ(0u, ms);
    PASS();
}

TEST clock_known_time(void)
{
    ctw_date d;
    uint32_t ms;
    clk_split(1792501490, &d, &ms);
    ASSERT_EQ(2026, d.year);
    ASSERT_EQ(10, d.month);
    ASSERT_EQ(20, d.day);
    PASS();
}

SUITE(clock_suite)
{
    RUN_TEST(clock_epoch);
    RUN_TEST(clock_known_time);
}
