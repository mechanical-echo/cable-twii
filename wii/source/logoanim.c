#include "logoanim.h"

#include "ui/tween.h"

#define LOGO_HEADER_X 19.2f
#define LOGO_HEADER_BAND_H 44.0f
#define LOGO_HEADER_H 28.0f
#define LOGO_CENTER_X 320.0f
#define LOGO_CENTER_Y 240.0f
#define LOGO_START_SCALE 0.6f

static float lerp(float a, float b, float p)
{
    return a + (b - a) * p;
}

static float phase(uint32_t t_ms, uint32_t start_ms, uint32_t end_ms)
{
    if (t_ms <= start_ms) return 0.0f;
    if (t_ms >= end_ms) return 1.0f;
    return tw_ease_out((float)(t_ms - start_ms) / (float)(end_ms - start_ms));
}

void logo_header_rect_for(float img_w, float img_h, float *x, float *y, float *w, float *h)
{
    *h = LOGO_HEADER_H;
    *w = img_h > 0.0f ? LOGO_HEADER_H * img_w / img_h : 0.0f;
    *x = LOGO_HEADER_X;
    *y = (LOGO_HEADER_BAND_H - LOGO_HEADER_H) / 2.0f;
}

logo_frame logo_frame_at(uint32_t t_ms, bool skipped, float img_w, float img_h)
{
    logo_frame f;
    float hx, hy, hw, hh;
    logo_header_rect_for(img_w, img_h, &hx, &hy, &hw, &hh);
    if (skipped || t_ms >= LOGO_HANDOFF_MS) {
        f.x = hx;
        f.y = hy;
        f.w = hw;
        f.h = hh;
        f.scale = img_w > 0.0f ? hw / img_w : 0.0f;
        f.alpha = 1.0f;
        f.shine = 1.0f;
        f.done = true;
        return f;
    }
    float intro = phase(t_ms, 0, LOGO_INTRO_END_MS);
    float move = phase(t_ms, LOGO_SHINE_END_MS, LOGO_HANDOFF_MS);
    float scale = lerp(LOGO_START_SCALE, 1.0f, intro);
    float cw = img_w * scale;
    float ch = img_h * scale;
    f.x = lerp(LOGO_CENTER_X - cw / 2.0f, hx, move);
    f.y = lerp(LOGO_CENTER_Y - ch / 2.0f, hy, move);
    f.w = lerp(cw, hw, move);
    f.h = lerp(ch, hh, move);
    f.scale = img_w > 0.0f ? f.w / img_w : 0.0f;
    f.alpha = intro;
    f.shine = phase(t_ms, LOGO_INTRO_END_MS, LOGO_SHINE_END_MS);
    f.done = false;
    return f;
}
