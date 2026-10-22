#include "glyphs.h"

#include <grrlib.h>

#include "glyph_home_png.h"
#include "glyph_play_png.h"
#include "draw.h"

static GRRLIB_texImg *tex[GLYPH_COUNT];

void glyphs_init(void)
{
    tex[GLYPH_HOME] = GRRLIB_LoadTexture(glyph_home_png);
    tex[GLYPH_PLAY] = GRRLIB_LoadTexture(glyph_play_png);
}

void ui_glyph_draw(ui_glyph g, float x, float y, float h, uint32_t color)
{
    GRRLIB_texImg *t = g < GLYPH_COUNT ? tex[g] : NULL;
    if (t == NULL || t->w == 0 || t->h == 0) return;
    float w = h * ui_pixel_x();
    GRRLIB_DrawImg(x, y, t, 0.0f, w / (float)t->w, h / (float)t->h, color);
}
