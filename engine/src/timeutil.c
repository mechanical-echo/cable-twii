#include "ctw/ctw.h"

/**
 * @throws never
 */
static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/**
 * @throws never
 */
static bool is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/**
 * @throws never
 */
static int days_in_month(int year, int month)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year)) {
        return 29;
    }
    return days[month - 1];
}

/**
 * @throws never
 */
int ctw_parse_hhmm(const char *s, uint32_t *out_ms)
{
    if (!is_digit(s[0]) || !is_digit(s[1]) || s[2] != ':' ||
        !is_digit(s[3]) || !is_digit(s[4]) || s[5] != '\0') {
        return -1;
    }
    uint32_t hh = (uint32_t)((s[0] - '0') * 10 + (s[1] - '0'));
    uint32_t mm = (uint32_t)((s[3] - '0') * 10 + (s[4] - '0'));
    if (hh > 23u || mm > 59u) {
        return -1;
    }
    *out_ms = (hh * 60u + mm) * 60000u;
    return 0;
}

/**
 * @throws never
 */
uint32_t ctw_date_key(ctw_date d)
{
    return (uint32_t)d.year * 10000u + (uint32_t)d.month * 100u + (uint32_t)d.day;
}

/**
 * @throws never
 */
ctw_date ctw_date_prev(ctw_date d)
{
    if (d.day > 1) {
        d.day--;
        return d;
    }
    if (d.month > 1) {
        d.month--;
    } else {
        d.month = 12;
        d.year--;
    }
    d.day = days_in_month(d.year, d.month);
    return d;
}

/**
 * @throws never
 */
ctw_date ctw_date_next(ctw_date d)
{
    if (d.day < days_in_month(d.year, d.month)) {
        d.day++;
        return d;
    }
    d.day = 1;
    if (d.month < 12) {
        d.month++;
    } else {
        d.month = 1;
        d.year++;
    }
    return d;
}

/**
 * @throws never
 */
int ctw_date_parse(const char *s, ctw_date *out)
{
    static const int digit_at[8] = {0, 1, 2, 3, 5, 6, 8, 9};
    for (int i = 0; i < 8; i++) {
        if (!is_digit(s[digit_at[i]])) {
            return -1;
        }
    }
    if (s[4] != '-' || s[7] != '-' || s[10] != '\0') {
        return -1;
    }
    int year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    int month = (s[5] - '0') * 10 + (s[6] - '0');
    int day = (s[8] - '0') * 10 + (s[9] - '0');
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > days_in_month(year, month)) {
        return -1;
    }
    out->year = year;
    out->month = month;
    out->day = day;
    return 0;
}
