#ifndef CTW_GUIDE_LAYOUT_H
#define CTW_GUIDE_LAYOUT_H

#include "aspect.h"
#include "ui/theme.h"

#define GL_HEADER_H THEME_HEADER_H
#define GL_TOP 58.7f
#define GL_BOTTOM (480.0f - THEME_SYSBAR_H - THEME_SYSBAR_HUMP - 5.2f)
#define GL_INFO_MIN_H 101.8f

typedef struct {
    asp_rect list;
    asp_rect preview;
    asp_rect info;
} guide_layout;

guide_layout guide_layout_for(float pixel_x);

#endif
