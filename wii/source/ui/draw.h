#ifndef CTW_UI_DRAW_H
#define CTW_UI_DRAW_H

#include <stdint.h>
#include <grrlib.h>

void ui_stripes(void);
void ui_panel(float x, float y, float w, float h, float radius, uint32_t fill, uint32_t border);
void ui_pill(float x, float y, float w, float h, const char *label, bool hot);
void ui_glow(float x, float y, float w, float h, float radius, float alpha);
void ui_image(GRRLIB_texImg *img, float x, float y, float w, float h, float alpha);
GRRLIB_texImg *ui_load_png_file(const char *path);

#endif
