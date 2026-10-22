#ifndef CTW_GUIDE_LAYOUT_H
#define CTW_GUIDE_LAYOUT_H

#include "aspect.h"

#define GL_HEADER_H 44.0f
#define GL_INFO_MIN_H 176.0f

typedef struct {
    asp_rect list;
    asp_rect preview;
    asp_rect info;
} guide_layout;

guide_layout guide_layout_for(float pixel_x);

#endif
