#ifndef CTW_UI_DRAW_H
#define CTW_UI_DRAW_H

#include <stdint.h>
#include <grrlib.h>

typedef struct {
    float pos;
    uint32_t color;
} ui_stop;

typedef enum { UI_SHADOW_NONE, UI_SHADOW_PANEL, UI_SHADOW_VIDEO } ui_shadow_kind;
typedef enum { UI_PILL_REST, UI_PILL_HOT, UI_PILL_PRESSED } ui_pill_state;

void ui_set_widescreen(bool widescreen);
float ui_pixel_x(void);
void ui_stripes(void);
void ui_stripes_colors(uint32_t a, uint32_t b);
void ui_stripes_rect(float x, float y, float w, float h, float alpha);
void ui_panel(float x, float y, float w, float h, float radius, uint32_t fill, uint32_t border);
void ui_pill(float x, float y, float w, float h, const char *label, bool hot);
void ui_pill_ex(float x, float y, float w, float h, const char *label, int px, ui_pill_state state, ui_shadow_kind shadow);
void ui_glow(float x, float y, float w, float h, float radius, float alpha);
void ui_ring(float x, float y, float w, float h, float radius, uint32_t color, float spread);
void ui_rounded(float x, float y, float w, float h, float radius, uint32_t color);
void ui_outline(float x, float y, float w, float h, float radius, float thickness, uint32_t color);
void ui_gradient_rect(float x, float y, float w, float h, uint32_t top, uint32_t bottom);
void ui_rounded_gradient(float x, float y, float w, float h, float radius, const ui_stop *stops, int n);
void ui_shadow(float x, float y, float w, float h, float radius, uint32_t color, float dy, float blur);
void ui_drop_shadow(float x, float y, float w, float h, float radius, ui_shadow_kind kind, float alpha_scale);
void ui_card(float x, float y, float w, float h, float radius, float mid, uint32_t bottom, ui_shadow_kind shadow);
void ui_card_alpha(float x, float y, float w, float h, float radius, float mid, uint32_t bottom, ui_shadow_kind shadow, float alpha);
void ui_corner_mask(float x, float y, float w, float h, float radius, uint32_t color);
void ui_bar(float x, float y, float w, float h, float fraction);
void ui_segments(float x, float y, float w, float h, int count, float gap, int lit, bool ring_newest);
float ui_number_pill_w(const char *text, int px);
void ui_number_pill(float x, float y, float h, const char *text, int px, bool tuned);
void ui_accent_tile(float x, float y, float w, float h, float radius);
void ui_battery(float x, float y, float pill_h, int lit, int label_px, float *out_w);
void ui_radial(float cx, float cy, float rx, float ry, uint32_t color);
void ui_disc(float x, float y, float h, uint32_t inner, uint32_t mid, uint32_t edge);
void ui_dashed_rounded(float x, float y, float w, float h, float radius, float thickness, float dash, float gap, uint32_t color);
void ui_image(GRRLIB_texImg *img, float x, float y, float w, float h, float alpha);
GRRLIB_texImg *ui_load_png_file(const char *path);

#endif
