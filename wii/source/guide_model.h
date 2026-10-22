#ifndef CTW_GUIDE_MODEL_H
#define CTW_GUIDE_MODEL_H

#include <stddef.h>
#include "ctw/ctw.h"

#define GM_HEADER_H 28.0f
#define GM_ROW_H 64.0f
#define GM_SPACING 6.0f
#define GM_EDGE_PX 24.0f
#define GM_EDGE_STEP 6.0f

typedef enum { GM_HEADER, GM_CHANNEL } gm_kind;

typedef struct {
    gm_kind kind;
    const char *category;
    const ctw_channel *channel;
    float y;
    float h;
} gm_row;

size_t gm_build(const ctw_db *db, gm_row *rows, size_t cap);
int gm_hit(const gm_row *rows, size_t n, float scroll, float list_top, float px, float py);
float gm_scroll_to(const gm_row *rows, size_t n, int index, float scroll, float view_h);
int gm_step(const gm_row *rows, size_t n, int index, int dir);
int gm_find(const gm_row *rows, size_t n, int channel_number);
float gm_content_h(const gm_row *rows, size_t n);
float gm_edge_scroll(float scroll, float py, float list_top, float view_h, float content_h);

#endif
