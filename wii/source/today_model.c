#include "today_model.h"

static td_kind kind_of(const ctw_airing *a)
{
    if (a->kind == CTW_AIR_AD) return TD_ADS;
    if (a->kind == CTW_AIR_UPNEXT) return TD_UPNEXT;
    return TD_EPISODE;
}

size_t td_build(const ctw_lineup *l, td_row *rows, size_t cap)
{
    if (l == NULL || l->items == NULL) return 0;
    size_t n = 0;
    for (size_t i = 0; i < l->count; i++) {
        const ctw_airing *a = &l->items[i];
        int64_t end = a->start_ms + (int64_t)a->duration_ms;
        if (a->kind == CTW_AIR_AD && n > 0 && rows[n - 1].kind == TD_ADS) {
            rows[n - 1].count++;
            rows[n - 1].total_ms += a->duration_ms;
            rows[n - 1].end_ms = end;
            continue;
        }
        if (n == cap) break;
        rows[n].kind = kind_of(a);
        rows[n].first = a;
        rows[n].count = 1;
        rows[n].start_ms = a->start_ms;
        rows[n].end_ms = end;
        rows[n].total_ms = a->duration_ms;
        n++;
    }
    return n;
}

int td_row_of(const td_row *rows, size_t n, const ctw_airing *a)
{
    if (a == NULL) return -1;
    for (size_t i = 0; i < n; i++) {
        if (a >= rows[i].first && a < rows[i].first + rows[i].count) return (int)i;
    }
    return -1;
}

float td_content_h(size_t n)
{
    if (n == 0) return 0.0f;
    return 2.0f * TD_PAD + (float)n * TD_ROW_H + (float)(n - 1) * TD_ROW_GAP;
}

float td_row_y(int index)
{
    return TD_PAD + (float)index * (TD_ROW_H + TD_ROW_GAP);
}

static float clamp_scroll(float s, size_t n, float view_h)
{
    float max = td_content_h(n) - view_h;
    if (s > max) s = max;
    if (s < 0.0f) s = 0.0f;
    return s;
}

float td_open_scroll(int now, size_t n, float view_h)
{
    if (now < 0) return 0.0f;
    return clamp_scroll(td_row_y(now - TD_NOW_SLOT) - TD_PAD, n, view_h);
}

float td_scroll_to(int index, float scroll, float view_h, size_t n)
{
    float y = td_row_y(index);
    if (y - TD_PAD < scroll) scroll = y - TD_PAD;
    else if (y + TD_ROW_H > scroll + view_h) scroll = y + TD_ROW_H - view_h;
    return clamp_scroll(scroll, n, view_h);
}

int td_hit(float scroll, float py_in_list, size_t n)
{
    float y = scroll + py_in_list - TD_PAD;
    if (y < 0.0f) return -1;
    float step = TD_ROW_H + TD_ROW_GAP;
    size_t i = (size_t)(y / step);
    if (i >= n) return -1;
    if (y - (float)i * step >= TD_ROW_H) return -1;
    return (int)i;
}
