#ifndef CTW_ASPECT_H
#define CTW_ASPECT_H

#include <stdbool.h>

typedef struct {
    float x, y, w, h;
} asp_rect;

float asp_pixel_x(bool widescreen);
asp_rect asp_fit(float src_aspect, bool widescreen, float x, float y, float w, float h);

#endif
