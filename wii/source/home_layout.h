#ifndef CTW_HOME_LAYOUT_H
#define CTW_HOME_LAYOUT_H

#include <stdint.h>
#include "aspect.h"

#define HOME_BATTERY_BARS 4
#define HOME_DIM_ALPHA 153

typedef enum {
    HOME_BTN_NONE,
    HOME_BTN_WII_MENU,
    HOME_BTN_HOMEBREW,
    HOME_BTN_RESUME
} home_btn;

typedef struct {
    asp_rect top;
    asp_rect bottom;
    asp_rect title;
    asp_rect wii_menu;
    asp_rect hbc;
    asp_rect resume;
    asp_rect battery[HOME_BATTERY_BARS];
} home_layout;

home_layout home_layout_make(void);
home_btn home_hit(const home_layout *l, float x, float y);
int home_battery_bars(uint8_t level);

#endif
