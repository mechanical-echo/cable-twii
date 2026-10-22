#ifndef CTW_UI_GLYPHS_H
#define CTW_UI_GLYPHS_H

#include <stdint.h>

typedef enum { GLYPH_HOME, GLYPH_PLAY, GLYPH_COUNT } ui_glyph;

void glyphs_init(void);
void ui_glyph_draw(ui_glyph g, float x, float y, float h, uint32_t color);

#endif
