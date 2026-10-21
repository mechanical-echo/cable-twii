#ifndef CTW_CLOCK_H
#define CTW_CLOCK_H

#include <stdint.h>
#include "ctw/ctw.h"

void clk_split(int64_t seconds, ctw_date *day, uint32_t *ms_of_day);
void clk_now(ctw_date *day, uint32_t *ms_of_day);

#endif
