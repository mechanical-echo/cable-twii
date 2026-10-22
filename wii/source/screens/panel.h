#ifndef CTW_SCREENS_PANEL_H
#define CTW_SCREENS_PANEL_H

#include "../app.h"
#include "../osd_layout.h"

osd_layout panel_layout(app *a, float offset_y);
void panel_draw(app *a, const osd_layout *l);
void panel_draw_volume(const osd_layout *l, int volume, osd_btn hot, osd_btn pressed);

#endif
