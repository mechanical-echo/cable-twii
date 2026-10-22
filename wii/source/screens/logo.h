#ifndef CTW_SCREENS_LOGO_H
#define CTW_SCREENS_LOGO_H

#include "../app.h"

void logo_enter(app *a);
void logo_update(app *a);
void logo_draw(app *a);
void logo_header_rect(float *x, float *y, float *w, float *h);
void logo_draw_header(void);

#endif
