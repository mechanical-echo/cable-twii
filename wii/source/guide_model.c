#include "guide_model.h"

#include <string.h>

static bool same_category(const char *a, const char *b)
{
    if (a == NULL || b == NULL) return a == b;
    return strcmp(a, b) == 0;
}

static bool category_seen(const ctw_db *db, size_t before, const char *category)
{
    for (size_t i = 0; i < before; i++) {
        if (same_category(ctw_db_channel_at(db, i)->category, category)) return true;
    }
    return false;
}

static void place(gm_row *rows, size_t i)
{
    rows[i].h = rows[i].kind == GM_HEADER ? GM_HEADER_H : GM_ROW_H;
    rows[i].y = i == 0 ? 0.0f : rows[i - 1].y + rows[i - 1].h + GM_SPACING;
}

size_t gm_build(const ctw_db *db, gm_row *rows, size_t cap)
{
    size_t n = 0;
    size_t count = ctw_db_channel_count(db);
    for (size_t i = 0; i < count && n < cap; i++) {
        const char *category = ctw_db_channel_at(db, i)->category;
        if (category_seen(db, i, category)) continue;
        rows[n].kind = GM_HEADER;
        rows[n].category = category;
        rows[n].channel = NULL;
        place(rows, n++);
        for (size_t j = i; j < count && n < cap; j++) {
            const ctw_channel *ch = ctw_db_channel_at(db, j);
            if (!same_category(ch->category, category)) continue;
            rows[n].kind = GM_CHANNEL;
            rows[n].category = category;
            rows[n].channel = ch;
            place(rows, n++);
        }
    }
    return n;
}

int gm_hit(const gm_row *rows, size_t n, float scroll, float list_top, float px, float py)
{
    (void)px;
    float cy = py - list_top + scroll;
    for (size_t i = 0; i < n; i++) {
        if (rows[i].kind != GM_CHANNEL) continue;
        if (cy >= rows[i].y && cy < rows[i].y + rows[i].h) return (int)i;
    }
    return -1;
}

float gm_scroll_to(const gm_row *rows, size_t n, int index, float scroll, float view_h)
{
    if (index < 0 || (size_t)index >= n) return scroll;
    const gm_row *r = &rows[index];
    if (r->y < scroll) return r->y;
    if (r->y + r->h > scroll + view_h) return r->y + r->h - view_h;
    return scroll;
}

int gm_step(const gm_row *rows, size_t n, int index, int dir)
{
    if (index < 0 || (size_t)index >= n) {
        for (size_t i = 0; i < n; i++) {
            if (rows[i].kind == GM_CHANNEL) return (int)i;
        }
        return -1;
    }
    for (int i = index + dir; i >= 0 && (size_t)i < n; i += dir) {
        if (rows[i].kind == GM_CHANNEL) return i;
    }
    return index;
}

int gm_find(const gm_row *rows, size_t n, int channel_number)
{
    for (size_t i = 0; i < n; i++) {
        if (rows[i].kind == GM_CHANNEL && rows[i].channel->number == channel_number) return (int)i;
    }
    return -1;
}

float gm_content_h(const gm_row *rows, size_t n)
{
    if (n == 0) return 0.0f;
    return rows[n - 1].y + rows[n - 1].h;
}

float gm_edge_scroll(float scroll, float py, float list_top, float view_h, float content_h)
{
    if (py >= list_top && py < list_top + GM_EDGE_PX) scroll -= GM_EDGE_STEP;
    else if (py > list_top + view_h - GM_EDGE_PX && py <= list_top + view_h) scroll += GM_EDGE_STEP;
    float max = content_h > view_h ? content_h - view_h : 0.0f;
    if (scroll > max) scroll = max;
    if (scroll < 0.0f) scroll = 0.0f;
    return scroll;
}
