#include "text.h"

#include <stdlib.h>
#include <string.h>
#include <grrlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "font_medium_ttf.h"
#include "font_bold_ttf.h"
#include "textfit.h"
#include "textcache.h"
#include "utf8.h"

#define ADV_SLOTS 2048
#define MAX_TEX_SIDE 1024

typedef struct {
    GRRLIB_texImg *tex;
    int ox;
    int oy;
    float width;
} text_img;

typedef struct {
    uint32_t key;
    int16_t adv;
    bool used;
} adv_slot;

static GRRLIB_ttfFont *fonts[2];
static tc_cache cache;
static adv_slot adv_cache[ADV_SLOTS];
static uint32_t *cp_buf;
static size_t cp_cap;

static void free_img(void *payload)
{
    text_img *img = (text_img *)payload;
    if (img == NULL) return;
    if (img->tex != NULL) {
        GX_DrawDone();
        GRRLIB_FreeTexture(img->tex);
    }
    free(img);
}

void ui_text_init(void)
{
    fonts[FONT_BODY] = GRRLIB_LoadTTF(font_medium_ttf, font_medium_ttf_size);
    fonts[FONT_BOLD] = GRRLIB_LoadTTF(font_bold_ttf, font_bold_ttf_size);
    tc_init(&cache, free_img);
}

static size_t decode(const char *s, size_t len, const uint32_t **out)
{
    if (len + 1 > cp_cap) {
        uint32_t *grown = (uint32_t *)realloc(cp_buf, (len + 1) * sizeof(uint32_t));
        if (grown == NULL) {
            *out = cp_buf;
            return 0;
        }
        cp_buf = grown;
        cp_cap = len + 1;
    }
    *out = cp_buf;
    return u8_decode(s, len, cp_buf, cp_cap);
}

static FT_Face face_for(ui_font f, int px)
{
    if (fonts[f] == NULL) return NULL;
    FT_Face face = (FT_Face)fonts[f]->face;
    if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)px) != 0) FT_Set_Pixel_Sizes(face, 0, 12);
    return face;
}

static int glyph_advance(FT_Face face, ui_font f, int px, FT_UInt gi)
{
    uint32_t key = ((uint32_t)f << 24) | (((uint32_t)px & 0xFFu) << 16) | (gi & 0xFFFFu);
    adv_slot *slot = &adv_cache[((key * 2654435761u) >> 21) & (ADV_SLOTS - 1)];
    if (slot->used && slot->key == key) return slot->adv;
    if (FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) != 0) return 0;
    int adv = (int)(face->glyph->advance.x >> 6);
    if (gi <= 0xFFFFu) {
        slot->key = key;
        slot->adv = (int16_t)adv;
        slot->used = true;
    }
    return adv;
}

static int kerning(ui_font f, FT_Face face, FT_UInt prev, FT_UInt gi)
{
    if (!fonts[f]->kerning || prev == 0 || gi == 0) return 0;
    FT_Vector delta;
    if (FT_Get_Kerning(face, prev, gi, FT_KERNING_DEFAULT, &delta) != 0) return 0;
    return (int)(delta.x >> 6);
}

static float measure(ui_font f, int px, const uint32_t *cps, size_t n)
{
    FT_Face face = face_for(f, px);
    if (face == NULL) return 0.0f;
    int pen = 0;
    FT_UInt prev = 0;
    for (size_t i = 0; i < n; i++) {
        FT_UInt gi = FT_Get_Char_Index(face, cps[i]);
        pen += kerning(f, face, prev, gi);
        pen += glyph_advance(face, f, px, gi);
        prev = gi;
    }
    return (float)pen;
}

static int round_up4(int v)
{
    return (v + 3) & ~3;
}

static void blit_glyph(GRRLIB_texImg *tex, const FT_Bitmap *bm, int gx, int gy, uint32_t rgba)
{
    uint32_t rgb = rgba & 0xFFFFFF00u;
    uint32_t a = rgba & 0xFFu;
    for (unsigned int row = 0; row < bm->rows; row++) {
        int ty = gy + (int)row;
        if (ty < 0 || ty >= (int)tex->h) continue;
        const unsigned char *src = bm->buffer + (int)row * bm->pitch;
        for (unsigned int col = 0; col < bm->width; col++) {
            int tx = gx + (int)col;
            if (tx < 0 || tx >= (int)tex->w) continue;
            uint32_t cov = src[col];
            if (cov == 0) continue;
            uint32_t prev = GRRLIB_GetPixelFromtexImg(tx, ty, tex) & 0xFFu;
            uint32_t alpha = cov * a / 255u;
            if (alpha < prev) alpha = prev;
            GRRLIB_SetPixelTotexImg(tx, ty, tex, rgb | alpha);
        }
    }
}

static text_img *render(ui_font f, int px, uint32_t rgba, const uint32_t *cps, size_t n)
{
    text_img *img = (text_img *)calloc(1, sizeof *img);
    if (img == NULL) return NULL;
    FT_Face face = face_for(f, px);
    if (face == NULL) return img;

    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool any = false;
    int pen = 0;
    FT_UInt prev = 0;
    for (size_t i = 0; i < n; i++) {
        FT_UInt gi = FT_Get_Char_Index(face, cps[i]);
        pen += kerning(f, face, prev, gi);
        prev = gi;
        if (FT_Load_Glyph(face, gi, FT_LOAD_RENDER) != 0) continue;
        FT_GlyphSlot slot = face->glyph;
        if (slot->bitmap.width > 0 && slot->bitmap.rows > 0) {
            int x0 = pen + slot->bitmap_left;
            int y0 = px - slot->bitmap_top;
            int x1 = x0 + (int)slot->bitmap.width;
            int y1 = y0 + (int)slot->bitmap.rows;
            if (!any || x0 < min_x) min_x = x0;
            if (!any || y0 < min_y) min_y = y0;
            if (!any || x1 > max_x) max_x = x1;
            if (!any || y1 > max_y) max_y = y1;
            any = true;
        }
        pen += (int)(slot->advance.x >> 6);
    }
    img->width = (float)pen;
    if (!any) return img;

    int tw = round_up4(max_x - min_x);
    int th = round_up4(max_y - min_y);
    if (tw > MAX_TEX_SIDE) tw = MAX_TEX_SIDE;
    if (th > MAX_TEX_SIDE) th = MAX_TEX_SIDE;
    GRRLIB_texImg *tex = GRRLIB_CreateEmptyTexture((u32)tw, (u32)th);
    if (tex == NULL || tex->data == NULL) {
        if (tex != NULL) free(tex);
        return img;
    }

    pen = 0;
    prev = 0;
    for (size_t i = 0; i < n; i++) {
        FT_UInt gi = FT_Get_Char_Index(face, cps[i]);
        pen += kerning(f, face, prev, gi);
        prev = gi;
        if (FT_Load_Glyph(face, gi, FT_LOAD_RENDER) != 0) continue;
        FT_GlyphSlot slot = face->glyph;
        blit_glyph(tex, &slot->bitmap, pen + slot->bitmap_left - min_x, px - slot->bitmap_top - min_y, rgba);
        pen += (int)(slot->advance.x >> 6);
    }
    GRRLIB_FlushTex(tex);
    GX_InvalidateTexAll();

    img->tex = tex;
    img->ox = min_x;
    img->oy = min_y;
    return img;
}

static const text_img *lookup(ui_font f, int px, uint32_t rgba, const char *utf8)
{
    tc_entry *e = tc_find(&cache, (int)f, px, rgba, utf8);
    if (e != NULL) return (const text_img *)e->payload;

    const uint32_t *cps;
    size_t n = decode(utf8, strlen(utf8), &cps);
    text_img *img = render(f, px, rgba, cps, n);
    if (img == NULL) return NULL;
    e = tc_insert(&cache, (int)f, px, rgba, utf8, img);
    if (e == NULL) {
        free_img(img);
        return NULL;
    }
    return img;
}

void ui_text(ui_font f, int px, float x, float y, const char *utf8, uint32_t rgba, ui_align a)
{
    if (utf8 == NULL || utf8[0] == '\0') return;
    const text_img *img = lookup(f, px, rgba, utf8);
    if (img == NULL || img->tex == NULL) return;
    float draw_x = x;
    if (a == ALIGN_CENTER) draw_x = x - img->width / 2.0f;
    else if (a == ALIGN_RIGHT) draw_x = x - img->width;
    GRRLIB_DrawImg((f32)(int)(draw_x + (float)img->ox), (f32)(int)(y + (float)img->oy), img->tex, 0.0f, 1.0f, 1.0f, 0xFFFFFFFF);
}

float ui_text_width(ui_font f, int px, const char *utf8)
{
    if (utf8 == NULL) return 0.0f;
    const uint32_t *cps;
    size_t n = decode(utf8, strlen(utf8), &cps);
    return measure(f, px, cps, n);
}

struct fit_ctx {
    ui_font f;
    int px;
};

static float ttf_width_prefix(const char *s, size_t cp, void *ctx)
{
    struct fit_ctx *fc = (struct fit_ctx *)ctx;
    if (cp == 0) return 0.0f;
    const uint32_t *cps;
    size_t n = decode(s, u8_prefix_bytes(s, cp), &cps);
    return measure(fc->f, fc->px, cps, n);
}

void ui_text_fit(ui_font f, int px, const char *utf8, float max_w, char *out, size_t out_size)
{
    struct fit_ctx fc = { f, px };
    tf_fit(utf8, max_w, ttf_width_prefix, &fc, out, out_size);
}

size_t ui_text_wrap(ui_font f, int px, const char *utf8, float max_w, size_t max_lines, tf_line *lines)
{
    struct fit_ctx fc = { f, px };
    return tf_wrap(utf8, max_w, max_lines, ttf_width_prefix, &fc, lines);
}
