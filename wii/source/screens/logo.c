#include "logo.h"

#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "logo_png.h"
#include "../input.h"
#include "../logoanim.h"
#include "../ui/draw.h"
#include "guide.h"

#define SHINE_SLOPE 0.5f
#define SHINE_SLICE 2.0f
#define SHINE_OUTER_W 44.0f
#define SHINE_INNER_W 16.0f
#define SHINE_OUTER_ALPHA 0.30f
#define SHINE_INNER_ALPHA 0.45f

static GRRLIB_texImg *tex_logo;
static GRRLIB_texImg *tex_shine;
static uint32_t start_ms;
static bool skipped;
static logo_frame frame;

static void ensure_loaded(void)
{
    if (tex_logo != NULL) return;
    tex_logo = GRRLIB_LoadTexture(logo_png);
    if (tex_logo == NULL) return;
    tex_shine = GRRLIB_CreateEmptyTexture(tex_logo->w, tex_logo->h);
    if (tex_shine == NULL) return;
    for (unsigned int y = 0; y < tex_logo->h; y++) {
        for (unsigned int x = 0; x < tex_logo->w; x++) {
            u32 alpha = GRRLIB_GetPixelFromtexImg(x, y, tex_logo) & 0xFFu;
            GRRLIB_SetPixelTotexImg(x, y, tex_shine, 0xFFFFFF00u | alpha);
        }
    }
    GRRLIB_FlushTex(tex_shine);
}

static float logo_w(void)
{
    return tex_logo != NULL ? (float)tex_logo->w : 0.0f;
}

static float logo_h(void)
{
    return tex_logo != NULL ? (float)tex_logo->h : 0.0f;
}

void logo_header_rect(float *x, float *y, float *w, float *h)
{
    ensure_loaded();
    logo_header_rect_for(logo_w(), logo_h(), x, y, w, h);
}

void logo_enter(app *a)
{
    ensure_loaded();
    start_ms = a->now_ms;
    skipped = false;
    frame = logo_frame_at(0, false, logo_w(), logo_h());
}

void logo_update(app *a)
{
    if (in_pressed(WPAD_BUTTON_A)) skipped = true;
    frame = logo_frame_at(a->now_ms - start_ms, skipped, logo_w(), logo_h());
    if (frame.done) app_go(a, SCR_GUIDE);
}

static void shine_band(float center, float width, float alpha)
{
    float w = logo_w(), h = logo_h();
    float sx = frame.w / w, sy = frame.h / h;
    u32 color = RGBA(0xFF, 0xFF, 0xFF, (u8)(alpha * frame.alpha * 255.0f));
    for (float u = 0.0f; u < w; u += SHINE_SLICE) {
        float pw = u + SHINE_SLICE > w ? w - u : SHINE_SLICE;
        float mid = u + pw / 2.0f;
        float v0 = h - (mid - center + width / 2.0f) / SHINE_SLOPE;
        float v1 = h - (mid - center - width / 2.0f) / SHINE_SLOPE;
        if (v0 < 0.0f) v0 = 0.0f;
        if (v1 > h) v1 = h;
        if (v1 <= v0) continue;
        float ph = v1 - v0;
        float dx = frame.x + u * sx, dy = frame.y + v0 * sy;
        GRRLIB_DrawPart(dx - pw * (1.0f - sx) / 2.0f, dy - ph * (1.0f - sy) / 2.0f,
                        u, v0, pw, ph, tex_shine, 0.0f, sx, sy, color);
    }
}

static void draw_shine(void)
{
    if (tex_shine == NULL || frame.shine <= 0.0f || frame.shine >= 1.0f) return;
    float lean = logo_h() * SHINE_SLOPE;
    float travel = logo_w() + lean + SHINE_OUTER_W;
    float center = -lean - SHINE_OUTER_W / 2.0f + travel * frame.shine;
    shine_band(center, SHINE_OUTER_W, SHINE_OUTER_ALPHA);
    shine_band(center, SHINE_INNER_W, SHINE_INNER_ALPHA);
}

void logo_draw(app *a)
{
    ui_stripes();
    if (frame.panels > 0.0f) guide_draw_intro(a, frame.panels);
    ui_image(tex_logo, frame.x, frame.y, frame.w, frame.h, frame.alpha);
    draw_shine();
}

void logo_draw_header(void)
{
    float x, y, w, h;
    logo_header_rect(&x, &y, &w, &h);
    ui_image(tex_logo, x, y, w, h, 1.0f);
}
