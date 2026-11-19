#ifndef CTW_SCREENS_GUIDE_INFO_H
#define CTW_SCREENS_GUIDE_INFO_H

#include "../app.h"
#include "../aspect.h"

void ginfo_reset(void);
void ginfo_draw_preview(app *a, asp_rect r, float expand, bool today_hot);
asp_rect ginfo_today_hint(asp_rect preview);
void ginfo_draw_info(app *a, const ctw_channel *ch, asp_rect r);

#endif
