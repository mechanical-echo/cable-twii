#ifndef CTW_GUIDE_TEXT_H
#define CTW_GUIDE_TEXT_H

#include <stddef.h>
#include <stdint.h>
#include "ctw/ctw.h"

void gt_hhmm(int64_t ms, char *out, size_t out_size);
void gt_date(ctw_date d, char *out, size_t out_size);
void gt_number(int number, char *out, size_t out_size);
void gt_countdown(uint32_t ms_left, char *out, size_t out_size);
void gt_at(int64_t start_ms, char *out, size_t out_size);
void gt_times(int64_t start_ms, uint32_t duration_ms, char *out, size_t out_size);
void gt_episode(const ctw_episode *ep, char *out, size_t out_size);
void gt_line(const ctw_airing *air, const ctw_airing *next, char *out, size_t out_size);
float gt_progress(const ctw_airing *air, uint32_t offset_ms);
const ctw_show *gt_line_show(const ctw_airing *air, const ctw_airing *next);
void gt_icon_path(const char *root, const ctw_channel *ch, const ctw_show *show, char *out, size_t out_size);

#endif
