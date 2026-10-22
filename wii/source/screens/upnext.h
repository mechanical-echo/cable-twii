#ifndef CTW_SCREENS_UPNEXT_H
#define CTW_SCREENS_UPNEXT_H

#include "../app.h"

void upnext_draw(app *a, const ctw_airing *gap, uint32_t gap_offset_ms, const ctw_airing *next, const ctw_airing *next_episode, int64_t ms_until_episode);

#endif
