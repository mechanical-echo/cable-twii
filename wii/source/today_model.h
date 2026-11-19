#ifndef CTW_TODAY_MODEL_H
#define CTW_TODAY_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include "ctw/ctw.h"

#define TD_ROW_H 32.0f
#define TD_ROW_GAP 2.0f
#define TD_PAD 4.0f
#define TD_NOW_SLOT 2

typedef enum { TD_EPISODE, TD_ADS, TD_UPNEXT } td_kind;

typedef struct {
    td_kind kind;
    const ctw_airing *first;
    size_t count;
    int64_t start_ms;
    int64_t end_ms;
    uint32_t total_ms;
} td_row;

size_t td_build(const ctw_lineup *l, td_row *rows, size_t cap);
int td_row_of(const td_row *rows, size_t n, const ctw_airing *a);
float td_content_h(size_t n);
float td_row_y(int index);
float td_open_scroll(int now, size_t n, float view_h);
float td_scroll_to(int index, float scroll, float view_h, size_t n);
int td_hit(float scroll, float py_in_list, size_t n);

#endif
