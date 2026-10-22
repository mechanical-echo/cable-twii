#ifndef CTW_OSD_LAYOUT_H
#define CTW_OSD_LAYOUT_H

#include "aspect.h"

#define OSD_VOLUME_SEGMENTS 10

typedef enum {
    OSD_BTN_NONE,
    OSD_BTN_PREV,
    OSD_BTN_NEXT,
    OSD_BTN_GUIDE,
    OSD_BTN_VOL_DOWN,
    OSD_BTN_VOL_UP
} osd_btn;

typedef struct {
    asp_rect panel;
    asp_rect name;
    asp_rect icon;
    asp_rect line;
    asp_rect times;
    asp_rect bar;
    asp_rect prev;
    asp_rect next;
    asp_rect guide;
    asp_rect vol_down;
    asp_rect vol_bar;
    asp_rect vol_up;
    asp_rect vol_panel;
    asp_rect vol_panel_bar;
} osd_layout;

osd_layout osd_layout_for(float offset_y);
osd_btn osd_hit(const osd_layout *l, float x, float y);
asp_rect osd_segment(asp_rect bar, int index);
int osd_segments_lit(int volume);

#endif
