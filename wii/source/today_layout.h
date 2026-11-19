#ifndef CTW_TODAY_LAYOUT_H
#define CTW_TODAY_LAYOUT_H

#include "aspect.h"

#define TL_HINT_H 26.7f
#define TL_HINT_DISC 19.6f
#define TL_HINT_PX 12

typedef struct {
    asp_rect card;
    asp_rect icon;
    asp_rect name;
    asp_rect date;
    asp_rect tag;
    asp_rect rule_top;
    asp_rect list;
    asp_rect scroll;
    asp_rect rule_bottom;
    asp_rect hints;
    asp_rect skip_note;
} today_layout;

typedef struct {
    float time_x;
    float icon_x;
    float icon_w;
    float show_x;
    float show_w;
    float code_x;
    float title_x;
    float title_w;
} today_cols;

today_layout today_layout_for(float pixel_x);
today_cols today_cols_for(asp_rect row, float time_w, float code_w, float pixel_x);
float today_hint_w(float label_w, float pixel_x);
asp_rect today_hint_full(asp_rect panel, float hint_w);
asp_rect today_hint_guide(asp_rect preview, float hint_w, float pixel_x);

#endif
