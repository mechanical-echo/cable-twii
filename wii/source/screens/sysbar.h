#ifndef CTW_SCREENS_SYSBAR_H
#define CTW_SCREENS_SYSBAR_H

#include <stdbool.h>
#include "../app.h"
#include "../sysbar_layout.h"
#include "../ui/press.h"

sysbar_btn sysbar_update(app *a, const sysbar_layout *l, bool channels, ps_state *ps);
void sysbar_draw(app *a, const sysbar_layout *l, bool channels, const ps_state *ps);

#endif
