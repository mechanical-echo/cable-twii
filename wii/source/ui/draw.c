#include "draw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "theme.h"
#include "text.h"
#include "../aspect.h"

#define MASK_SIDE 32
#define RADIAL_SIDE 64
#define CORNER_STEPS 12
#define SHADOW_LAYERS 3
#define PILL_LABEL_INSET 16.0f
#define HIGHLIGHT_PX 1.0f
#define PILL_SHADE_PX 3.0f
#define PILL_PRESS_DY 2.0f
#define BATTERY_LABEL "P1"
#define BATTERY_LABEL_GAP 5.3f
#define BATTERY_BARS 4
#define NUMBER_PILL_MIN_W 22.0f
#define NUMBER_PILL_PAD 10.0f

static float pixel_x = 1.0f;
static GRRLIB_texImg *corner_mask;
static GRRLIB_texImg *radial_tex;
static uint32_t stripe_a = THEME_STRIPE_A;
static uint32_t stripe_b = THEME_STRIPE_B;
static float corner_cos[CORNER_STEPS + 1];
static float corner_sin[CORNER_STEPS + 1];
static bool corners_ready;

static const ui_stop ACCENT_STOPS[] = {
    { 0.0f, THEME_ACCENT_LIGHT }, { 0.55f, THEME_ACCENT }, { 1.0f, THEME_ACCENT_DARK }
};
static const ui_stop BAR_STOPS[] = {
    { 0.0f, THEME_ACCENT_LIGHT }, { 0.6f, THEME_ACCENT }, { 1.0f, THEME_ACCENT_DARK }
};
static const ui_stop PILL_STOPS[] = {
    { 0.0f, THEME_PILL_FILL_TOP }, { 0.46f, THEME_PILL_FILL_TOP }, { 0.54f, THEME_PILL_BAND_MID }, { 1.0f, THEME_PILL_BAND_BOTTOM }
};
static const ui_stop PILL_PRESSED_STOPS[] = {
    { 0.0f, THEME_PILL_BAND_BOTTOM }, { 0.46f, THEME_PILL_BAND_MID }, { 0.54f, THEME_PILL_FILL_TOP }, { 1.0f, THEME_PILL_FILL_TOP }
};
static const ui_stop BATTERY_STOPS[] = { { 0.0f, THEME_ACCENT_LIGHT }, { 1.0f, THEME_ACCENT } };

void ui_set_widescreen(bool widescreen)
{
    pixel_x = asp_pixel_x(widescreen);
}

float ui_pixel_x(void)
{
    return pixel_x;
}

static void ensure_corners(void)
{
    if (corners_ready) return;
    for (int i = 0; i <= CORNER_STEPS; i++) {
        float a = (float)i / (float)CORNER_STEPS * (float)M_PI / 2.0f;
        corner_cos[i] = cosf(a);
        corner_sin[i] = sinf(a);
    }
    corners_ready = true;
}

static uint32_t with_alpha(uint32_t color, float alpha)
{
    float a = (float)(color & 0xFFu) * (alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha));
    return (color & 0xFFFFFF00u) | (uint32_t)(a + 0.5f);
}

static uint32_t lerp_color(uint32_t a, uint32_t b, float t)
{
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        float ca = (float)((a >> shift) & 0xFFu), cb = (float)((b >> shift) & 0xFFu);
        out |= ((uint32_t)(ca + (cb - ca) * t + 0.5f) & 0xFFu) << shift;
    }
    return out;
}

static uint32_t grad_at(const ui_stop *s, int n, float t)
{
    if (n <= 0) return 0;
    if (t <= s[0].pos) return s[0].color;
    for (int i = 0; i + 1 < n; i++) {
        if (t <= s[i + 1].pos) {
            float span = s[i + 1].pos - s[i].pos;
            float k = span > 0.0f ? (t - s[i].pos) / span : 1.0f;
            return lerp_color(s[i].color, s[i + 1].color, k);
        }
    }
    return s[n - 1].color;
}

static void quad(float x0, float y0, float x1, float y1, uint32_t top, uint32_t bottom)
{
    if (x1 <= x0 || y1 <= y0) return;
    guVector v[4] = { { x0, y0, 0.0f }, { x1, y0, 0.0f }, { x1, y1, 0.0f }, { x0, y1, 0.0f } };
    u32 c[4] = { top, top, bottom, bottom };
    GRRLIB_GXEngine(v, c, 4, GX_QUADS);
}

void ui_gradient_rect(float x, float y, float w, float h, uint32_t top, uint32_t bottom)
{
    quad(x, y, x + w, y + h, top, bottom);
}

uint32_t ui_grad_at(const ui_stop *stops, int n, float t)
{
    return grad_at(stops, n, t);
}

void ui_quad(const float *xs, const float *ys, const uint32_t *colors)
{
    guVector v[4];
    u32 c[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = xs[i];
        v[i].y = ys[i];
        v[i].z = 0.0f;
        c[i] = colors[i];
    }
    GRRLIB_GXEngine(v, c, 4, GX_QUADS);
}

void ui_polyline(const float *xs, const float *ys, int n, float thickness, uint32_t color)
{
    for (int i = 0; i + 1 < n; i++) {
        float dx = xs[i + 1] - xs[i], dy = ys[i + 1] - ys[i];
        float len = sqrtf(dx * dx + dy * dy);
        if (len <= 0.0f) continue;
        float nx = -dy / len * thickness / 2.0f, ny = dx / len * thickness / 2.0f;
        guVector v[4] = { { xs[i] + nx, ys[i] + ny, 0.0f }, { xs[i + 1] + nx, ys[i + 1] + ny, 0.0f },
                          { xs[i + 1] - nx, ys[i + 1] - ny, 0.0f }, { xs[i] - nx, ys[i] - ny, 0.0f } };
        u32 c[4] = { color, color, color, color };
        GRRLIB_GXEngine(v, c, 4, GX_QUADS);
    }
}

static void corner(float cx, float cy, float rx, float ry, int q, uint32_t color)
{
    ensure_corners();
    guVector v[CORNER_STEPS + 2];
    u32 c[CORNER_STEPS + 2];
    float sx = (q == 0 || q == 3) ? -1.0f : 1.0f;
    float sy = (q == 0 || q == 1) ? -1.0f : 1.0f;
    v[0].x = cx;
    v[0].y = cy;
    v[0].z = 0.0f;
    c[0] = color;
    for (int i = 0; i <= CORNER_STEPS; i++) {
        v[i + 1].x = cx + sx * rx * corner_cos[i];
        v[i + 1].y = cy + sy * ry * corner_sin[i];
        v[i + 1].z = 0.0f;
        c[i + 1] = color;
    }
    GRRLIB_GXEngine(v, c, CORNER_STEPS + 2, GX_TRIANGLEFAN);
}

static void clamp_radii(float radius, float w, float h, float *rx, float *ry)
{
    if (radius < 0.0f) radius = 0.0f;
    *rx = radius * pixel_x;
    *ry = radius;
    if (*rx > w / 2.0f) *rx = w / 2.0f;
    if (*ry > h / 2.0f) *ry = h / 2.0f;
}

static void rounded_rect(float x, float y, float w, float h, float radius, uint32_t color)
{
    if (w <= 0.0f || h <= 0.0f) return;
    float rx, ry;
    clamp_radii(radius, w, h, &rx, &ry);
    if (rx <= 0.0f || ry <= 0.0f) {
        GRRLIB_Rectangle(x, y, w, h, color, 1);
        return;
    }
    GRRLIB_Rectangle(x + rx, y, w - 2 * rx, h, color, 1);
    GRRLIB_Rectangle(x, y + ry, rx, h - 2 * ry, color, 1);
    GRRLIB_Rectangle(x + w - rx, y + ry, rx, h - 2 * ry, color, 1);
    corner(x + rx, y + ry, rx, ry, 0, color);
    corner(x + w - rx, y + ry, rx, ry, 1, color);
    corner(x + w - rx, y + h - ry, rx, ry, 2, color);
    corner(x + rx, y + h - ry, rx, ry, 3, color);
}

void ui_rounded(float x, float y, float w, float h, float radius, uint32_t color)
{
    rounded_rect(x, y, w, h, radius, color);
}

static void arc_band(float cx, float cy, float rx, float ry, float irx, float iry, int q, uint32_t color)
{
    ensure_corners();
    guVector v[2 * (CORNER_STEPS + 1)];
    u32 c[2 * (CORNER_STEPS + 1)];
    float sx = (q == 0 || q == 3) ? -1.0f : 1.0f;
    float sy = (q == 0 || q == 1) ? -1.0f : 1.0f;
    for (int i = 0; i <= CORNER_STEPS; i++) {
        v[2 * i].x = cx + sx * rx * corner_cos[i];
        v[2 * i].y = cy + sy * ry * corner_sin[i];
        v[2 * i].z = 0.0f;
        v[2 * i + 1].x = cx + sx * irx * corner_cos[i];
        v[2 * i + 1].y = cy + sy * iry * corner_sin[i];
        v[2 * i + 1].z = 0.0f;
        c[2 * i] = color;
        c[2 * i + 1] = color;
    }
    GRRLIB_GXEngine(v, c, 2 * (CORNER_STEPS + 1), GX_TRIANGLESTRIP);
}

void ui_outline(float x, float y, float w, float h, float radius, float thickness, uint32_t color)
{
    if (w <= 0.0f || h <= 0.0f) return;
    float rx, ry;
    clamp_radii(radius, w, h, &rx, &ry);
    float tx = thickness * pixel_x, ty = thickness;
    if (tx > rx) tx = rx;
    if (ty > ry) ty = ry;
    if (rx <= 0.0f || ry <= 0.0f) {
        GRRLIB_Rectangle(x, y, w, ty, color, 1);
        GRRLIB_Rectangle(x, y + h - ty, w, ty, color, 1);
        GRRLIB_Rectangle(x, y + ty, tx, h - 2 * ty, color, 1);
        GRRLIB_Rectangle(x + w - tx, y + ty, tx, h - 2 * ty, color, 1);
        return;
    }
    GRRLIB_Rectangle(x + rx, y, w - 2 * rx, ty, color, 1);
    GRRLIB_Rectangle(x + rx, y + h - ty, w - 2 * rx, ty, color, 1);
    GRRLIB_Rectangle(x, y + ry, tx, h - 2 * ry, color, 1);
    GRRLIB_Rectangle(x + w - tx, y + ry, tx, h - 2 * ry, color, 1);
    arc_band(x + rx, y + ry, rx, ry, rx - tx, ry - ty, 0, color);
    arc_band(x + w - rx, y + ry, rx, ry, rx - tx, ry - ty, 1, color);
    arc_band(x + w - rx, y + h - ry, rx, ry, rx - tx, ry - ty, 2, color);
    arc_band(x + rx, y + h - ry, rx, ry, rx - tx, ry - ty, 3, color);
}

static void grad_bands(float x, float w, float y, float h, float ya, float yb, const ui_stop *s, int n)
{
    if (w <= 0.0f || h <= 0.0f || yb <= ya) return;
    float prev = 0.0f;
    for (int i = 0; i <= n; i++) {
        float next = i < n ? s[i].pos : 1.0f;
        if (next < prev) next = prev;
        if (next > 1.0f) next = 1.0f;
        float y0 = y + prev * h, y1 = y + next * h;
        if (y0 < ya) y0 = ya;
        if (y1 > yb) y1 = yb;
        if (y1 > y0) quad(x, y0, x + w, y1, grad_at(s, n, (y0 - y) / h), grad_at(s, n, (y1 - y) / h));
        prev = next;
    }
}

void ui_rounded_gradient(float x, float y, float w, float h, float radius, const ui_stop *stops, int n)
{
    if (w <= 0.0f || h <= 0.0f || n <= 0) return;
    float rx, ry;
    clamp_radii(radius, w, h, &rx, &ry);
    if (rx <= 0.0f || ry <= 0.0f) {
        grad_bands(x, w, y, h, y, y + h, stops, n);
        return;
    }
    grad_bands(x + rx, w - 2 * rx, y, h, y, y + h, stops, n);
    grad_bands(x, rx, y, h, y + ry, y + h - ry, stops, n);
    grad_bands(x + w - rx, rx, y, h, y + ry, y + h - ry, stops, n);
    uint32_t top = grad_at(stops, n, ry / h), bottom = grad_at(stops, n, 1.0f - ry / h);
    corner(x + rx, y + ry, rx, ry, 0, top);
    corner(x + w - rx, y + ry, rx, ry, 1, top);
    corner(x + w - rx, y + h - ry, rx, ry, 2, bottom);
    corner(x + rx, y + h - ry, rx, ry, 3, bottom);
}

void ui_shadow(float x, float y, float w, float h, float radius, uint32_t color, float dy, float blur)
{
    uint32_t a = (color & 0xFFu) / SHADOW_LAYERS;
    if (a == 0) a = 1;
    uint32_t layer = (color & 0xFFFFFF00u) | a;
    for (int i = 1; i <= SHADOW_LAYERS; i++) {
        float e = blur * (float)i / (float)SHADOW_LAYERS;
        rounded_rect(x - e * pixel_x, y + dy - e, w + 2.0f * e * pixel_x, h + 2.0f * e, radius + e, layer);
    }
}

void ui_drop_shadow(float x, float y, float w, float h, float radius, ui_shadow_kind kind, float alpha_scale)
{
    if (kind == UI_SHADOW_PANEL) {
        ui_shadow(x, y, w, h, radius, with_alpha(THEME_PANEL_SHADOW, alpha_scale), THEME_PANEL_SHADOW_DY, THEME_PANEL_SHADOW_BLUR);
    } else if (kind == UI_SHADOW_VIDEO) {
        ui_shadow(x, y, w, h, radius, with_alpha(THEME_VIDEO_SHADOW, alpha_scale), THEME_VIDEO_SHADOW_DY, THEME_VIDEO_SHADOW_BLUR);
    }
}

void ui_ring(float x, float y, float w, float h, float radius, uint32_t color, float spread)
{
    rounded_rect(x - spread * pixel_x, y - spread, w + 2.0f * spread * pixel_x, h + 2.0f * spread, radius + spread, color);
}

void ui_glow(float x, float y, float w, float h, float radius, float alpha)
{
    ui_ring(x, y, w, h, radius, with_alpha(THEME_ACCENT, alpha), THEME_RING_SPREAD);
}

void ui_stripes_colors(uint32_t a, uint32_t b)
{
    stripe_a = a;
    stripe_b = b;
}

void ui_stripes(void)
{
    int y = 0;
    bool a = true;
    while (y < 480) {
        uint32_t color = a ? stripe_a : stripe_b;
        GRRLIB_Rectangle(0, y, 640, THEME_STRIPE_PX, color, 1);
        y += THEME_STRIPE_PX;
        a = !a;
    }
}

void ui_stripes_rect(float x, float y, float w, float h, float alpha)
{
    if (alpha <= 0.0f) return;
    if (alpha > 1.0f) alpha = 1.0f;
    uint8_t a = (uint8_t)(alpha * 255.0f);
    int y0 = (int)y;
    int y1 = (int)(y + h + 0.999f);
    if (y0 < 0) y0 = 0;
    if (y1 > 480) y1 = 480;
    int row = y0 - y0 % THEME_STRIPE_PX;
    while (row < y1) {
        uint32_t color = (row / THEME_STRIPE_PX) % 2 == 0 ? stripe_a : stripe_b;
        int top = row < y0 ? y0 : row;
        int bottom = row + THEME_STRIPE_PX > y1 ? y1 : row + THEME_STRIPE_PX;
        GRRLIB_Rectangle(x, (float)top, w, (float)(bottom - top), (color & 0xFFFFFF00u) | a, 1);
        row += THEME_STRIPE_PX;
    }
}

static void ensure_corner_mask(void)
{
    if (corner_mask != NULL) return;
    corner_mask = GRRLIB_CreateEmptyTexture(MASK_SIDE, MASK_SIDE);
    if (corner_mask == NULL) return;
    for (int py = 0; py < MASK_SIDE; py++) {
        for (int px = 0; px < MASK_SIDE; px++) {
            float dx = (float)MASK_SIDE - ((float)px + 0.5f);
            float dy = (float)MASK_SIDE - ((float)py + 0.5f);
            float edge = sqrtf(dx * dx + dy * dy) - (float)MASK_SIDE + 0.5f;
            float cover = edge < 0.0f ? 0.0f : (edge > 1.0f ? 1.0f : edge);
            GRRLIB_SetPixelTotexImg(px, py, corner_mask, 0xFFFFFF00u | (uint32_t)(cover * 255.0f));
        }
    }
    GRRLIB_FlushTex(corner_mask);
    GX_InvalidateTexAll();
}

void ui_corner_mask(float x, float y, float w, float h, float radius, uint32_t color)
{
    if (radius <= 0.0f) return;
    ensure_corner_mask();
    if (corner_mask == NULL) return;
    float rx = radius * pixel_x, ry = radius;
    if (rx > w / 2.0f) rx = w / 2.0f;
    if (ry > h / 2.0f) ry = h / 2.0f;
    float sx = rx / (float)MASK_SIDE, sy = ry / (float)MASK_SIDE;
    GRRLIB_DrawImg(x, y, corner_mask, 0.0f, sx, sy, color);
    GRRLIB_DrawImg(x + w, y, corner_mask, 0.0f, -sx, sy, color);
    GRRLIB_DrawImg(x, y + h, corner_mask, 0.0f, sx, -sy, color);
    GRRLIB_DrawImg(x + w, y + h, corner_mask, 0.0f, -sx, -sy, color);
}

static void ensure_radial(void)
{
    if (radial_tex != NULL) return;
    radial_tex = GRRLIB_CreateEmptyTexture(RADIAL_SIDE, RADIAL_SIDE);
    if (radial_tex == NULL) return;
    float half = (float)RADIAL_SIDE / 2.0f;
    for (int py = 0; py < RADIAL_SIDE; py++) {
        for (int px = 0; px < RADIAL_SIDE; px++) {
            float dx = ((float)px + 0.5f - half) / half, dy = ((float)py + 0.5f - half) / half;
            float d = sqrtf(dx * dx + dy * dy);
            float t = d >= 1.0f ? 0.0f : 1.0f - d;
            float a = t * t * (3.0f - 2.0f * t);
            GRRLIB_SetPixelTotexImg(px, py, radial_tex, 0xFFFFFF00u | (uint32_t)(a * 255.0f));
        }
    }
    GRRLIB_FlushTex(radial_tex);
    GX_InvalidateTexAll();
}

void ui_radial(float cx, float cy, float rx, float ry, uint32_t color)
{
    if (rx <= 0.0f || ry <= 0.0f) return;
    ensure_radial();
    if (radial_tex == NULL) return;
    GRRLIB_DrawImg(cx - rx, cy - ry, radial_tex, 0.0f, 2.0f * rx / (float)RADIAL_SIDE, 2.0f * ry / (float)RADIAL_SIDE, color);
}

void ui_disc(float x, float y, float h, uint32_t inner, uint32_t mid, uint32_t edge)
{
    float w = h * pixel_x;
    float cx = x + w / 2.0f, cy = y + h / 2.0f;
    rounded_rect(x, y, w, h, h / 2.0f, edge);
    ui_radial(cx, cy, w / 2.0f, h / 2.0f, mid);
    ui_radial(cx, cy - h * 0.1f, w * 0.32f, h * 0.32f, inner);
}

void ui_bar(float x, float y, float w, float h, float fraction)
{
    if (w <= 0.0f) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    float r = h / 2.0f;
    rounded_rect(x, y, w, h, r, THEME_TRACK_INSET);
    rounded_rect(x, y + HIGHLIGHT_PX, w, h - HIGHLIGHT_PX, r, THEME_BAR_TRACK);
    float fw = w * fraction;
    if (fw >= h * pixel_x) ui_rounded_gradient(x, y, fw, h, r, BAR_STOPS, 3);
}

void ui_segments(float x, float y, float w, float h, int count, float gap, int lit, bool ring_newest)
{
    if (count <= 0 || w <= 0.0f) return;
    float seg_w = (w - gap * (float)(count - 1)) / (float)count;
    if (seg_w <= 0.0f) return;
    for (int i = 0; i < count; i++) {
        float sx = x + (seg_w + gap) * (float)i;
        if (i < lit) {
            if (ring_newest && i == lit - 1) ui_ring(sx, y, seg_w, h, 3.0f, THEME_RING, 2.0f);
            rounded_rect(sx, y, seg_w, h, 3.0f, 0xFFFFFF80u);
            ui_rounded_gradient(sx, y + HIGHLIGHT_PX, seg_w, h - HIGHLIGHT_PX, 3.0f, ACCENT_STOPS, 3);
        } else {
            rounded_rect(sx, y, seg_w, h, 3.0f, THEME_TRACK_INSET);
            rounded_rect(sx, y + HIGHLIGHT_PX, seg_w, h - HIGHLIGHT_PX, 3.0f, THEME_BAR_TRACK);
        }
    }
}

float ui_number_pill_w(const char *text, int px)
{
    float w = ui_text_width(FONT_BOLD, px, text) + NUMBER_PILL_PAD;
    return w < NUMBER_PILL_MIN_W ? NUMBER_PILL_MIN_W : w;
}

void ui_number_pill(float x, float y, float h, const char *text, int px, bool tuned)
{
    float w = ui_number_pill_w(text, px);
    float r = h / 2.0f;
    float ix = x + 2.0f * pixel_x, iy = y + 2.0f, iw = w - 4.0f * pixel_x, ih = h - 4.0f;
    if (tuned) {
        ui_outline(x, y, w, h, r, 2.0f, THEME_ACCENT_DARK);
        ui_rounded_gradient(ix, iy, iw, ih, r - 2.0f, ACCENT_STOPS, 3);
    } else {
        ui_outline(x, y, w, h, r, 2.0f, THEME_PANEL_BORDER);
    }
    ui_text(FONT_BOLD, px, x + w / 2.0f, y + h / 2.0f - (float)px / 2.0f, text, tuned ? 0xFFFFFFFFu : THEME_TEXT_SECONDARY, ALIGN_CENTER);
}

void ui_accent_tile(float x, float y, float w, float h, float radius)
{
    ui_rounded_gradient(x, y, w, h, radius, ACCENT_STOPS, 3);
}

void ui_battery(float x, float y, float pill_h, int lit, int label_px, float *out_w)
{
    float lw = ui_text_width(FONT_BOLD, label_px, BATTERY_LABEL);
    ui_text(FONT_BOLD, label_px, x, y + (pill_h - (float)label_px) / 2.0f, BATTERY_LABEL, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    float px0 = x + lw + BATTERY_LABEL_GAP;
    float bar_w = pill_h * 0.183f, gap = pill_h * 0.138f, pad = pill_h * 0.245f;
    float pw = 2.0f * pad + (float)BATTERY_BARS * bar_w + (float)(BATTERY_BARS - 1) * gap;
    float r = pill_h / 2.0f;
    ui_outline(px0, y, pw, pill_h, r, 2.0f, THEME_PILL_BORDER);
    rounded_rect(px0 + 2.0f * pixel_x, y + 2.0f, pw - 4.0f * pixel_x, pill_h - 4.0f, r - 2.0f, THEME_PANEL_FILL);
    float bh = pill_h * 0.55f, by = y + (pill_h - bh) / 2.0f;
    for (int i = 0; i < BATTERY_BARS; i++) {
        float bx = px0 + pad + (bar_w + gap) * (float)i;
        if (i < lit) ui_rounded_gradient(bx, by, bar_w, bh, 3.0f, BATTERY_STOPS, 2);
        else rounded_rect(bx, by, bar_w, bh, 3.0f, THEME_BAR_TRACK);
    }
    if (out_w != NULL) *out_w = lw + BATTERY_LABEL_GAP + pw;
}

void ui_card_alpha(float x, float y, float w, float h, float radius, float mid, uint32_t bottom, ui_shadow_kind shadow, float alpha)
{
    ui_drop_shadow(x, y, w, h, radius, shadow, alpha);
    ui_outline(x, y, w, h, radius, 2.0f, with_alpha(THEME_PANEL_BORDER, alpha));
    float ix = x + 2.0f * pixel_x, iy = y + 2.0f, iw = w - 4.0f * pixel_x, ih = h - 4.0f;
    float ir = radius > 2.0f ? radius - 2.0f : 0.0f;
    rounded_rect(ix, iy, iw, ih, ir, with_alpha(THEME_PANEL_HIGHLIGHT, alpha));
    ui_stop stops[3] = { { 0.0f, with_alpha(THEME_PANEL_FILL, alpha) }, { mid, with_alpha(THEME_PANEL_FILL, alpha) }, { 1.0f, with_alpha(bottom, alpha) } };
    if (mid > 0.0f) {
        ui_rounded_gradient(ix, iy + HIGHLIGHT_PX, iw, ih - HIGHLIGHT_PX, ir, stops, 3);
    } else {
        stops[1] = stops[2];
        ui_rounded_gradient(ix, iy + HIGHLIGHT_PX, iw, ih - HIGHLIGHT_PX, ir, stops, 2);
    }
}

void ui_card(float x, float y, float w, float h, float radius, float mid, uint32_t bottom, ui_shadow_kind shadow)
{
    ui_card_alpha(x, y, w, h, radius, mid, bottom, shadow, 1.0f);
}

void ui_panel(float x, float y, float w, float h, float radius, uint32_t fill, uint32_t border)
{
    rounded_rect(x, y, w, h, radius, border);
    rounded_rect(x + 2, y + 2, w - 4, h - 4, radius > 2.0f ? radius - 2.0f : 0.0f, fill);
}

void ui_pill_ex(float x, float y, float w, float h, const char *label, int px, ui_pill_state state, ui_shadow_kind shadow)
{
    if (state == UI_PILL_PRESSED) y += PILL_PRESS_DY;
    float radius = h / 2.0f;
    if (state != UI_PILL_PRESSED) {
        if (shadow == UI_SHADOW_PANEL) ui_shadow(x, y, w, h, radius, THEME_PILL_SHADOW, 1.0f, 4.0f);
        else if (shadow == UI_SHADOW_VIDEO) ui_drop_shadow(x, y, w, h, radius, UI_SHADOW_VIDEO, 1.0f);
    }
    if (state == UI_PILL_HOT) ui_ring(x, y, w, h, radius, THEME_RING_STRONG, THEME_RING_SPREAD);
    if (state == UI_PILL_PRESSED) ui_ring(x, y, w, h, radius, THEME_RING, THEME_RING_SPREAD);
    float b = state == UI_PILL_REST ? 2.0f : 3.0f;
    ui_outline(x, y, w, h, radius, b, state == UI_PILL_REST ? THEME_PILL_BORDER : THEME_ACCENT);
    float ix = x + b * pixel_x, iy = y + b, iw = w - 2.0f * b * pixel_x, ih = h - 2.0f * b;
    float ir = radius - b;
    if (state == UI_PILL_PRESSED) {
        ui_rounded_gradient(ix, iy, iw, ih, ir, PILL_PRESSED_STOPS, 4);
        rounded_rect(ix, iy, iw, PILL_SHADE_PX, ir, THEME_PILL_SHADE);
    } else {
        rounded_rect(ix, iy, iw, ih, ir, THEME_PANEL_HIGHLIGHT);
        ui_rounded_gradient(ix, iy + HIGHLIGHT_PX, iw, ih - HIGHLIGHT_PX, ir, PILL_STOPS, 4);
    }
    if (label != NULL) {
        char fitted[64];
        float inset = w / 4.0f < PILL_LABEL_INSET ? w / 4.0f : PILL_LABEL_INSET;
        ui_text_fit(FONT_BOLD, px, label, w - inset, fitted, sizeof fitted);
        ui_text(FONT_BOLD, px, x + w / 2.0f, y + h / 2.0f - (float)px / 2.0f, fitted, THEME_TEXT_PRIMARY, ALIGN_CENTER);
    }
}

void ui_pill(float x, float y, float w, float h, const char *label, bool hot)
{
    ui_pill_ex(x, y, w, h, label, 14, hot ? UI_PILL_HOT : UI_PILL_REST, UI_SHADOW_PANEL);
}

static void dash_quad(float x0, float y0, float x1, float y1, float thickness, uint32_t color)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len <= 0.0f) return;
    float nx = -dy / len * thickness / 2.0f, ny = dx / len * thickness / 2.0f;
    guVector v[4] = { { x0 + nx, y0 + ny, 0.0f }, { x1 + nx, y1 + ny, 0.0f }, { x1 - nx, y1 - ny, 0.0f }, { x0 - nx, y0 - ny, 0.0f } };
    u32 c[4] = { color, color, color, color };
    GRRLIB_GXEngine(v, c, 4, GX_QUADS);
}

void ui_dashed_rounded(float x, float y, float w, float h, float radius, float thickness, float dash, float gap, uint32_t color)
{
    if (w <= 0.0f || h <= 0.0f || dash <= 0.0f) return;
    ensure_corners();
    float rx, ry;
    clamp_radii(radius, w, h, &rx, &ry);
    guVector path[4 * (CORNER_STEPS + 1) + 1];
    int n = 0;
    for (int i = 0; i <= CORNER_STEPS; i++) path[n++] = (guVector){ x + w - rx + rx * corner_sin[i], y + ry - ry * corner_cos[i], 0.0f };
    for (int i = 0; i <= CORNER_STEPS; i++) path[n++] = (guVector){ x + w - rx + rx * corner_cos[i], y + h - ry + ry * corner_sin[i], 0.0f };
    for (int i = 0; i <= CORNER_STEPS; i++) path[n++] = (guVector){ x + rx - rx * corner_sin[i], y + h - ry + ry * corner_cos[i], 0.0f };
    for (int i = 0; i <= CORNER_STEPS; i++) path[n++] = (guVector){ x + rx - rx * corner_cos[i], y + ry - ry * corner_sin[i], 0.0f };
    path[n++] = path[0];
    bool on = true;
    float left = dash;
    for (int i = 0; i + 1 < n; i++) {
        float ax = path[i].x, ay = path[i].y, bx = path[i + 1].x, by = path[i + 1].y;
        float len = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        float pos = 0.0f;
        while (pos < len) {
            float step = left < len - pos ? left : len - pos;
            float t0 = pos / len, t1 = (pos + step) / len;
            if (on) dash_quad(ax + (bx - ax) * t0, ay + (by - ay) * t0, ax + (bx - ax) * t1, ay + (by - ay) * t1, thickness, color);
            pos += step;
            left -= step;
            if (left <= 0.0f) {
                on = !on;
                left = on ? dash : gap;
            }
        }
    }
}

void ui_image(GRRLIB_texImg *img, float x, float y, float w, float h, float alpha)
{
    if (img == NULL) return;
    uint8_t a = (uint8_t)(alpha < 0.0f ? 0 : (alpha > 1.0f ? 255 : alpha * 255.0f));
    float sx = img->w > 0 ? w / (float)img->w : 1.0f;
    float sy = img->h > 0 ? h / (float)img->h : 1.0f;
    GRRLIB_DrawImg(x, y, img, 0.0f, sx, sy, RGBA(0xFF, 0xFF, 0xFF, a));
}

GRRLIB_texImg *ui_load_png_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size) {
        free(buf);
        return NULL;
    }
    GRRLIB_texImg *tex = GRRLIB_LoadTexturePNG(buf);
    free(buf);
    if (tex == NULL) return NULL;
    if (tex->data == NULL || tex->w == 0 || tex->h == 0 || tex->w > 128 || tex->h > 128) {
        GRRLIB_FreeTexture(tex);
        return NULL;
    }
    GX_InvalidateTexAll();
    return tex;
}
