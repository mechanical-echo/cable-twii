#ifndef CTW_SCREENS_NOTICE_H
#define CTW_SCREENS_NOTICE_H

#include "../app.h"
#include "../aspect.h"

void notice_show(app *a, const char *text);
void notice_restart(app *a);
void notice_draw(const app *a);
void notice_weak(asp_rect video);

#endif
