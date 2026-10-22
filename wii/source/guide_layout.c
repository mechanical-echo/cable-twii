#include "guide_layout.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define INSET_X 0.03f
#define INSET_BOTTOM 0.04f
#define GAP 0.03f
#define LIST_COL 0.42f
#define WIDE_W 16.0f
#define WIDE_H 9.0f

guide_layout guide_layout_for(float pixel_x)
{
    guide_layout l;
    float left = SCREEN_W * INSET_X;
    float right = SCREEN_W - left;
    float top = GL_HEADER_H;
    float bottom = SCREEN_H * (1.0f - INSET_BOTTOM);
    float gw = right - left, gh = bottom - top;
    float col_gap = gw * GAP, row_gap = gh * GAP;

    l.list = (asp_rect){ left, top, gw * LIST_COL, gh };

    float rx = left + l.list.w + col_gap;
    float rw = right - rx;
    float pw = rw;
    float ph = pw / pixel_x * WIDE_H / WIDE_W;
    float max_ph = gh - row_gap - GL_INFO_MIN_H;
    if (ph > max_ph) {
        ph = max_ph;
        pw = ph * pixel_x * WIDE_W / WIDE_H;
    }
    l.preview = (asp_rect){ rx + (rw - pw) / 2.0f, top, pw, ph };

    float iy = top + ph + row_gap;
    l.info = (asp_rect){ rx, iy, rw, bottom - iy };
    return l;
}
