#ifndef CTW_SCREENS_SETTINGS_SCREEN_H
#define CTW_SCREENS_SETTINGS_SCREEN_H

#include "../app.h"
#include "../net/srvlib.h"

void settings_enter(app *a);
void settings_update(app *a);
void settings_draw(app *a);
void settings_test_done(app *a, srv_result r, const srv_info *info);

#endif
