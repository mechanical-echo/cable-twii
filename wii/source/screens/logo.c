#include "logo.h"

#include <math.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "logo_png.h"
#include "../input.h"
#include "../logoanim.h"
#include "../ui/draw.h"
#include "../ui/theme.h"
#include "guide.h"

#define BLOOM_CX 320.0f
#define BLOOM_CY 216.0f
#define BLOOM_RX 448.0f
#define BLOOM_RY 336.0f
#define PANEL_SHADOW 0x344A6029u
#define PANEL_SHADOW_DY 7.0f
#define PANEL_SHADOW_BLUR 20.0f
#define SHINE_W 82.8f
#define SHINE_TILT 0.404f
#define SHINE_COLOR 0xEEFAFF00u
#define SHINE_ALPHA 0.95f
#define DOT_Y 328.9f
#define DOT_H 8.9f
#define DOT_X0 304.0f
#define DOT_STEP 12.67f
#define DOT_RING 2.2f
#define DOTS 3

static GRRLIB_texImg *tex_logo;
static uint32_t start_ms;
static bool skipped;
static logo_frame frame;

static void ensure_loaded(void)
{
    if (tex_logo != NULL) return;
    tex_logo = GRRLIB_LoadTexture(logo_png);
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

static uint32_t faded(uint32_t color, float alpha)
{
    float a = (float)(color & 0xFFu) * (alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha));
    return (color & 0xFFFFFF00u) | (uint32_t)(a + 0.5f);
}

static void draw_panel(void)
{
    float a = frame.panel_alpha;
    if (a <= 0.0f) return;
    ui_radial(BLOOM_CX, BLOOM_CY, BLOOM_RX, BLOOM_RY, faded(0xFFFFFFFFu, a));
    ui_radial(BLOOM_CX, BLOOM_CY, BLOOM_RX * 0.6f, BLOOM_RY * 0.6f, faded(0xFFFFFFFFu, a));
    ui_shadow(frame.px, frame.py, frame.pw, frame.ph, THEME_PANEL_RADIUS, faded(PANEL_SHADOW, a), PANEL_SHADOW_DY, PANEL_SHADOW_BLUR);
    ui_ring(frame.px, frame.py, frame.pw, frame.ph, THEME_PANEL_RADIUS, faded(THEME_RING, a), THEME_RING_SPREAD);
    ui_card_alpha(frame.px, frame.py, frame.pw, frame.ph, THEME_PANEL_RADIUS, 0.5f, THEME_GLOSS_SOFT, UI_SHADOW_NONE, a);
}

static void draw_shine(void)
{
    if (frame.shine <= 0.0f || frame.shine >= 1.0f || frame.panel_alpha <= 0.0f) return;
    float px = frame.px, py = frame.py, pw = frame.pw, ph = frame.ph;
    float lean = ph / 2.0f * SHINE_TILT;
    float half = SHINE_W / 2.0f;
    float cx = px - half - lean + (pw + SHINE_W + 2.0f * lean) * frame.shine;
    uint32_t edge = SHINE_COLOR;
    uint32_t mid = faded(SHINE_COLOR | 0xFFu, SHINE_ALPHA * frame.panel_alpha);
    GRRLIB_ClipDrawing((u32)(px + 2.0f), (u32)(py + 2.0f), (u32)(pw - 4.0f), (u32)(ph - 4.0f));
    float xs[4], ys[4];
    uint32_t cs[4];
    xs[0] = cx - half + lean; ys[0] = py; cs[0] = edge;
    xs[1] = cx + lean; ys[1] = py; cs[1] = mid;
    xs[2] = cx - lean; ys[2] = py + ph; cs[2] = mid;
    xs[3] = cx - half - lean; ys[3] = py + ph; cs[3] = edge;
    ui_quad(xs, ys, cs);
    xs[0] = cx + lean; ys[0] = py; cs[0] = mid;
    xs[1] = cx + half + lean; ys[1] = py; cs[1] = edge;
    xs[2] = cx + half - lean; ys[2] = py + ph; cs[2] = edge;
    xs[3] = cx - lean; ys[3] = py + ph; cs[3] = mid;
    ui_quad(xs, ys, cs);
    GRRLIB_ClipReset();
}

static void draw_dots(void)
{
    float a = frame.panel_alpha;
    if (a <= 0.0f) return;
    float w = DOT_H * ui_pixel_x();
    for (int i = 0; i < DOTS; i++) {
        float x = DOT_X0 + DOT_STEP * (float)i;
        uint32_t color = i == frame.dot ? THEME_ACCENT : (i == (frame.dot + 1) % DOTS ? THEME_DOT_2 : THEME_DOT_3);
        if (i == frame.dot) ui_ring(x, DOT_Y, w, DOT_H, DOT_H / 2.0f, faded(THEME_RING, a), DOT_RING);
        ui_rounded(x, DOT_Y, w, DOT_H, DOT_H / 2.0f, faded(color, a));
    }
}

void logo_draw(app *a)
{
    ui_stripes();
    if (frame.panels > 0.0f) guide_draw_intro(a, frame.panels);
    draw_panel();
    ui_image(tex_logo, frame.x, frame.y, frame.w, frame.h, frame.alpha);
    draw_shine();
    draw_dots();
}

void logo_draw_header(void)
{
    float x, y, w, h;
    logo_header_rect(&x, &y, &w, &h);
    ui_image(tex_logo, x, y, w, h, 1.0f);
}
