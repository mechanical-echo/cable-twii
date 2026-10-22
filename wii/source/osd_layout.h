#ifndef CTW_OSD_LAYOUT_H
#define CTW_OSD_LAYOUT_H

#include <stdbool.h>
#include "aspect.h"

#define OSD_VOLUME_SEGMENTS 10
#define OSD_CHIP_NUMBER_PX 24
#define OSD_CHIP_NUMBER_MAX_W 200.0f
#define OSD_CHIP_NAME_PX 13
#define OSD_CHIP_NAME_MAX_W 300.0f

typedef enum {
    OSD_BTN_NONE,
    OSD_BTN_VOL_DOWN,
    OSD_BTN_VOL_UP
} osd_btn;

typedef struct {
    asp_rect panel;
    asp_rect icon;
    asp_rect name;
    asp_rect number;
    asp_rect line;
    asp_rect start;
    asp_rect bar;
    asp_rect end;
    asp_rect divider;
    asp_rect next_icon;
    asp_rect next_label;
    asp_rect next_title;
    asp_rect next_at;
    asp_rect countdown;
    asp_rect vol_label;
    asp_rect vol_panel;
    asp_rect vol_down;
    asp_rect vol_bar;
    asp_rect vol_up;
} osd_layout;

typedef struct {
    asp_rect chip;
    asp_rect tile;
    asp_rect icon;
    asp_rect name_pill;
} osd_chip;

osd_layout osd_layout_for(float offset_y, float pixel_x, float name_w, float start_w);
osd_chip osd_chip_layout(float number_w, float name_w, float pixel_x);
osd_btn osd_hit(const osd_layout *l, float x, float y, bool volume_visible);
asp_rect osd_scrim(void);
asp_rect osd_segment(asp_rect bar, int index);
int osd_segments_lit(int volume);

#endif
