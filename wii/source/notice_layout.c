#include "notice_layout.h"

#include "ui/theme.h"

#define NOTICE_H 28.4f
#define NOTICE_Y 10.0f
#define NOTICE_BELOW_GAP 8.0f
#define NOTICE_CLEAR 14.0f
#define NOTICE_EDGE 6.0f
#define SCREEN_W 640.0f

asp_rect notice_pill_rect(float w, asp_rect logo, float pixel_x)
{
    float clear = NOTICE_CLEAR * pixel_x;
    float left = logo.x + logo.w + clear;
    float right = THEME_HEADER_BATTERY_X - clear;
    if (w <= right - left) {
        float x = (SCREEN_W - w) / 2.0f;
        if (x < left) x = left;
        if (x + w > right) x = right - w;
        return (asp_rect){ x, NOTICE_Y, w, NOTICE_H };
    }
    float x = (SCREEN_W - w) / 2.0f;
    if (x < NOTICE_EDGE) x = NOTICE_EDGE;
    return (asp_rect){ x, THEME_HEADER_H + NOTICE_BELOW_GAP, w, NOTICE_H };
}
