#include "draw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "theme.h"
#include "text.h"
#include "../aspect.h"

#define MASK_SIDE 32

static float pixel_x = 1.0f;
static GRRLIB_texImg *corner_mask;

void ui_set_widescreen(bool widescreen)
{
    pixel_x = asp_pixel_x(widescreen);
}

float ui_pixel_x(void)
{
    return pixel_x;
}

void ui_stripes(void)
{
    int y = 0;
    bool a = true;
    while (y < 480) {
        uint32_t color = a ? THEME_STRIPE_A : THEME_STRIPE_B;
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
        uint32_t color = (row / THEME_STRIPE_PX) % 2 == 0 ? THEME_STRIPE_A : THEME_STRIPE_B;
        int top = row < y0 ? y0 : row;
        int bottom = row + THEME_STRIPE_PX > y1 ? y1 : row + THEME_STRIPE_PX;
        GRRLIB_Rectangle(x, (float)top, w, (float)(bottom - top), (color & 0xFFFFFF00u) | a, 1);
        row += THEME_STRIPE_PX;
    }
}

static void rounded_rect(float x, float y, float w, float h, float radius, uint32_t color)
{
    if (radius < 0.0f) radius = 0.0f;
    float rx = radius * pixel_x;
    float ry = radius;
    if (rx > w / 2.0f) rx = w / 2.0f;
    if (ry > h / 2.0f) ry = h / 2.0f;

    GRRLIB_Rectangle(x + rx, y, w - 2 * rx, h, color, 1);
    if (rx > 0.0f && ry > 0.0f) {
        GRRLIB_Rectangle(x, y + ry, rx, h - 2 * ry, color, 1);
        GRRLIB_Rectangle(x + w - rx, y + ry, rx, h - 2 * ry, color, 1);
        GRRLIB_Ellipse(x + rx, y + ry, rx, ry, color, 1);
        GRRLIB_Ellipse(x + w - rx, y + ry, rx, ry, color, 1);
        GRRLIB_Ellipse(x + rx, y + h - ry, rx, ry, color, 1);
        GRRLIB_Ellipse(x + w - rx, y + h - ry, rx, ry, color, 1);
    }
}

void ui_rounded(float x, float y, float w, float h, float radius, uint32_t color)
{
    rounded_rect(x, y, w, h, radius, color);
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

void ui_bar(float x, float y, float w, float h, float fraction)
{
    if (w <= 0.0f) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    rounded_rect(x, y, w, h, h / 2.0f, THEME_BAR_TRACK);
    float fw = w * fraction;
    if (fw >= h * pixel_x) rounded_rect(x, y, fw, h, h / 2.0f, THEME_ACCENT);
}

void ui_panel(float x, float y, float w, float h, float radius, uint32_t fill, uint32_t border)
{
    rounded_rect(x, y, w, h, radius, border);
    rounded_rect(x + 2, y + 2, w - 4, h - 4, radius > 2.0f ? radius - 2.0f : 0.0f, fill);
}

void ui_pill(float x, float y, float w, float h, const char *label, bool hot)
{
    float radius = h / 2.0f;
    uint32_t border = hot ? THEME_ACCENT : THEME_PILL_BORDER;
    if (hot) ui_glow(x, y, w, h, radius, 0.6f);
    rounded_rect(x, y, w, h, radius, border);
    rounded_rect(x + 1, y + 1, w - 2, h - 2, radius > 1.0f ? radius - 1.0f : 0.0f, THEME_PILL_FILL_TOP);
    int steps = 4;
    for (int i = 0; i < steps; i++) {
        float t = (float)i / (float)(steps - 1);
        uint8_t r = (uint8_t)(0xFF + t * (0xE6 - 0xFF));
        uint8_t g = (uint8_t)(0xFF + t * (0xEA - 0xFF));
        uint8_t b = (uint8_t)(0xFF + t * (0xEE - 0xFF));
        float band_h = (h - 2) / (float)steps;
        GRRLIB_Rectangle(x + 1, y + 1 + band_h * i, w - 2, band_h + 1, RGBA(r, g, b, 0xFF), 1);
    }
    if (label != NULL) {
        char fitted[64];
        ui_text_fit(FONT_BODY, 14, label, w - 16.0f, fitted, sizeof fitted);
        ui_text(FONT_BODY, 14, x + w / 2.0f, y + h / 2.0f - 7.0f, fitted, THEME_TEXT_PRIMARY, ALIGN_CENTER);
    }
}

void ui_glow(float x, float y, float w, float h, float radius, float alpha)
{
    uint8_t a = (uint8_t)(alpha < 0.0f ? 0 : (alpha > 1.0f ? 255 : alpha * 255.0f));
    uint32_t color = RGBA(0x34, 0xBE, 0xED, a);
    rounded_rect(x - 4, y - 4, w + 8, h + 8, radius + 4, color);
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
