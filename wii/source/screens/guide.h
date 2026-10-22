#ifndef CTW_SCREENS_GUIDE_H
#define CTW_SCREENS_GUIDE_H

#include "../app.h"

void guide_prepare(app *a);
void guide_enter(app *a);
void guide_update(app *a);
void guide_draw(app *a);
void guide_draw_intro(app *a, float progress);

#endif
