#ifndef CTW_SYSBAR_LAYOUT_H
#define CTW_SYSBAR_LAYOUT_H

#include <stdbool.h>
#include "aspect.h"

typedef enum {
    SYSBAR_BTN_NONE,
    SYSBAR_BTN_HOME,
    SYSBAR_BTN_PREV,
    SYSBAR_BTN_NEXT
} sysbar_btn;

typedef struct {
    asp_rect body;
    asp_rect hump;
    float slope_w;
    asp_rect clock;
    asp_rect date;
    asp_rect home_disc;
    asp_rect home_label;
    asp_rect home_hit;
    asp_rect prev;
    asp_rect next;
} sysbar_layout;

sysbar_layout sysbar_layout_for(float pixel_x);
sysbar_btn sysbar_hit(const sysbar_layout *l, float x, float y, bool channels);
float sysbar_top_at(const sysbar_layout *l, float x);

#endif
