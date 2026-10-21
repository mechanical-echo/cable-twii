#include <stdlib.h>
#include <string.h>
#include "greatest/greatest.h"
#include "ctw/ctw.h"

TEST hhmm_parsing(void)
{
    uint32_t ms;
    ASSERT_EQ(0, ctw_parse_hhmm("07:00", &ms));
    ASSERT_EQ(25200000u, ms);
    ASSERT_EQ(0, ctw_parse_hhmm("23:59", &ms));
    ASSERT_EQ(86340000u, ms);
    ASSERT_EQ(0, ctw_parse_hhmm("00:00", &ms));
    ASSERT_EQ(0u, ms);
    ASSERT_EQ(-1, ctw_parse_hhmm("24:00", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("07:60", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("7:00", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("07:00 ", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("07-00", &ms));
    ASSERT_EQ(-1, ctw_parse_hhmm("", &ms));
    PASS();
}

TEST date_arithmetic(void)
{
    ctw_date p = ctw_date_prev((ctw_date){2024, 3, 1});
    ASSERT_EQ(2024, p.year);
    ASSERT_EQ(2, p.month);
    ASSERT_EQ(29, p.day);
    p = ctw_date_prev((ctw_date){2026, 1, 1});
    ASSERT_EQ(2025, p.year);
    ASSERT_EQ(12, p.month);
    ASSERT_EQ(31, p.day);
    p = ctw_date_next((ctw_date){2026, 2, 28});
    ASSERT_EQ(3, p.month);
    ASSERT_EQ(1, p.day);
    p = ctw_date_next((ctw_date){2026, 12, 31});
    ASSERT_EQ(2027, p.year);
    ASSERT_EQ(1, p.month);
    ASSERT_EQ(1, p.day);
    p = ctw_date_next((ctw_date){2024, 2, 28});
    ASSERT_EQ(29, p.day);
    p = ctw_date_next((ctw_date){1900, 2, 28});
    ASSERT_EQ(3, p.month);
    ASSERT_EQ(1, p.day);
    p = ctw_date_next((ctw_date){2000, 2, 28});
    ASSERT_EQ(2, p.month);
    ASSERT_EQ(29, p.day);
    ASSERT_EQ(20261020u, ctw_date_key((ctw_date){2026, 10, 20}));
    PASS();
}

TEST date_parse_valid(void)
{
    ctw_date d;
    ASSERT_EQ(0, ctw_date_parse("2026-10-20", &d));
    ASSERT_EQ(2026, d.year);
    ASSERT_EQ(10, d.month);
    ASSERT_EQ(20, d.day);
    ASSERT_EQ(0, ctw_date_parse("2024-02-29", &d));
    ASSERT_EQ(29, d.day);
    ASSERT_EQ(0, ctw_date_parse("2000-02-29", &d));
    ASSERT_EQ(29, d.day);
    PASS();
}

TEST date_parse_rejects_invalid(void)
{
    ctw_date d;
    ASSERT_EQ(-1, ctw_date_parse("2023-02-29", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026-1-05", &d));
    ASSERT_EQ(-1, ctw_date_parse("1900-02-29", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026-13-01", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026-00-10", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026-04-31", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026-10-00", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026-10-20 ", &d));
    ASSERT_EQ(-1, ctw_date_parse("2026/10/20", &d));
    ASSERT_EQ(-1, ctw_date_parse("", &d));
    PASS();
}

static int parse_exact(const char *text, ctw_date *d)
{
    size_t n = strlen(text) + 1u;
    char *buf = malloc(n);
    int rc;
    if (!buf) return -2;
    memcpy(buf, text, n);
    rc = ctw_date_parse(buf, d);
    free(buf);
    return rc;
}

TEST date_parse_rejects_short(void)
{
    ctw_date d;
    ASSERT_EQ(-1, parse_exact("2026", &d));
    ASSERT_EQ(-1, parse_exact("2026-12", &d));
    ASSERT_EQ(-1, parse_exact("", &d));
    ASSERT_EQ(-1, parse_exact("2026-12-0", &d));
    ASSERT_EQ(0, parse_exact("2026-12-01", &d));
    PASS();
}

SUITE(time_suite)
{
    RUN_TEST(hhmm_parsing);
    RUN_TEST(date_arithmetic);
    RUN_TEST(date_parse_valid);
    RUN_TEST(date_parse_rejects_invalid);
    RUN_TEST(date_parse_rejects_short);
}
