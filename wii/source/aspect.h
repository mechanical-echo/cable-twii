#ifndef CTW_ASPECT_H
#define CTW_ASPECT_H

#include <stdbool.h>

#define ASP_MODES 3

typedef struct {
    float x, y, w, h;
} asp_rect;

typedef enum { ASP_FIT, ASP_14_9, ASP_FILL } asp_mode;

float asp_pixel_x(bool widescreen);
asp_rect asp_fit(float src_aspect, bool widescreen, float x, float y, float w, float h);
float asp_shown(float src_aspect, bool widescreen, asp_mode mode, float amount);
asp_rect asp_crop(float src_aspect, float shown_aspect);

#endif
