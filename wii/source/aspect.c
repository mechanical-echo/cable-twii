#include "aspect.h"

#define EFB_W 640.0f
#define EFB_H 480.0f
#define TV_WIDE (16.0f / 9.0f)

float asp_pixel_x(bool widescreen)
{
    if (!widescreen) return 1.0f;
    return (EFB_W / EFB_H) / TV_WIDE;
}

asp_rect asp_fit(float src_aspect, bool widescreen, float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    if (src_aspect <= 0.0f || w <= 0.0f || h <= 0.0f) return r;
    float target = w / asp_pixel_x(widescreen) / h;
    if (src_aspect >= target) {
        r.h = h * target / src_aspect;
    } else {
        r.w = w * src_aspect / target;
    }
    r.x = x + (w - r.w) / 2.0f;
    r.y = y + (h - r.h) / 2.0f;
    return r;
}
