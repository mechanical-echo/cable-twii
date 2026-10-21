#include "clock.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <time.h>

void clk_split(int64_t seconds, ctw_date *day, uint32_t *ms_of_day)
{
    time_t t = (time_t)seconds;
    struct tm tmv;
    gmtime_r(&t, &tmv);
    day->year = tmv.tm_year + 1900;
    day->month = tmv.tm_mon + 1;
    day->day = tmv.tm_mday;
    *ms_of_day = (uint32_t)((tmv.tm_hour * 3600 + tmv.tm_min * 60 + tmv.tm_sec) * 1000);
}

void clk_now(ctw_date *day, uint32_t *ms_of_day)
{
    clk_split((int64_t)time(NULL), day, ms_of_day);
}
