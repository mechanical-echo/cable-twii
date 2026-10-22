#ifndef CTW_SCREENS_GUIDE_LIST_H
#define CTW_SCREENS_GUIDE_LIST_H

#include <stddef.h>
#include "../app.h"
#include "../aspect.h"
#include "../guide_model.h"

#define GLIST_PAD 8.0f

int glist_init(size_t row_count);
void glist_view(asp_rect panel, float *top, float *height);
void glist_draw(app *a, const gm_row *rows, size_t n, int highlight, float scroll, asp_rect panel, int tuned_channel);

#endif
