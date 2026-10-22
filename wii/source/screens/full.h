#ifndef CTW_SCREENS_FULL_H
#define CTW_SCREENS_FULL_H

#include "../app.h"

void tune(app *a, int channel);
void full_enter(app *a);
void full_update(app *a);
void full_draw(app *a);
bool full_cursor_visible(const app *a);

#endif
