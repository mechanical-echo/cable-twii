#ifndef CTW_UI_CURSOR_H
#define CTW_UI_CURSOR_H

#include <stdbool.h>

void cur_init(void);
void cur_update(void);
void cur_draw(void);
bool cur_visible(void);
float cur_x(void);
float cur_y(void);

#endif
