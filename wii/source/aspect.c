#include "aspect.h"

#define EFB_W 640.0f
#define EFB_H 480.0f
#define TV_WIDE (16.0f / 9.0f)
#define FOURTEEN_NINE (14.0f / 9.0f)

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

static float clamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float asp_shown(float src_aspect, bool widescreen, asp_mode mode, float amount)
{
    if (src_aspect <= 0.0f) return src_aspect;
    float tv = (EFB_W / EFB_H) / asp_pixel_x(widescreen);
    float narrow = src_aspect < tv ? src_aspect : tv;
    float wide = src_aspect < tv ? tv : src_aspect;
    float target = src_aspect;
    if (mode == ASP_FILL) target = tv;
    else if (mode == ASP_14_9) target = clamp(FOURTEEN_NINE, narrow, wide);
    return src_aspect + (target - src_aspect) * clamp(amount, 0.0f, 1.0f);
}

asp_rect asp_crop(float src_aspect, float shown_aspect)
{
    asp_rect r = { 0.0f, 0.0f, 1.0f, 1.0f };
    if (src_aspect <= 0.0f || shown_aspect <= 0.0f) return r;
    if (shown_aspect < src_aspect) {
        r.w = shown_aspect / src_aspect;
        r.x = (1.0f - r.w) / 2.0f;
    } else if (shown_aspect > src_aspect) {
        r.h = src_aspect / shown_aspect;
        r.y = (1.0f - r.h) / 2.0f;
    }
    return r;
}
