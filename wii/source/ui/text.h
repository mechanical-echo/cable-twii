#ifndef CTW_UI_TEXT_H
#define CTW_UI_TEXT_H

#include <stddef.h>
#include <stdint.h>

typedef enum { FONT_BODY, FONT_BOLD } ui_font;
typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } ui_align;

void ui_text_init(void);
void ui_text(ui_font f, int px, float x, float y, const char *utf8, uint32_t rgba, ui_align a);
float ui_text_width(ui_font f, int px, const char *utf8);
void ui_text_fit(ui_font f, int px, const char *utf8, float max_w, char *out, size_t out_size);

#endif
