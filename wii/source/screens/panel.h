#ifndef CTW_SCREENS_PANEL_H
#define CTW_SCREENS_PANEL_H

#include "../app.h"
#include "../osd_layout.h"

void panel_draw(app *a, const osd_layout *l, osd_btn hot);
void panel_draw_volume(const osd_layout *l, int volume);

#endif
