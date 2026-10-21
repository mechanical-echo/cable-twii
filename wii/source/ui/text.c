#include "text.h"

#include <string.h>
#include <grrlib.h>

#include "font_medium_ttf.h"
#include "font_bold_ttf.h"
#include "textfit.h"

static GRRLIB_ttfFont *fonts[2];

void ui_text_init(void)
{
    fonts[FONT_BODY] = GRRLIB_LoadTTF(font_medium_ttf, font_medium_ttf_size);
    fonts[FONT_BOLD] = GRRLIB_LoadTTF(font_bold_ttf, font_bold_ttf_size);
}

static float x_for_align(ui_font f, int px, const char *utf8, float x, ui_align a)
{
    if (a == ALIGN_LEFT) return x;
    float w = GRRLIB_WidthTTF(fonts[f], utf8, px);
    return a == ALIGN_CENTER ? x - w / 2.0f : x - w;
}

void ui_text(ui_font f, int px, float x, float y, const char *utf8, uint32_t rgba, ui_align a)
{
    float draw_x = x_for_align(f, px, utf8, x, a);
    GRRLIB_PrintfTTF((int)draw_x, (int)y, fonts[f], utf8, (unsigned int)px, rgba);
}

float ui_text_width(ui_font f, int px, const char *utf8)
{
    return (float)GRRLIB_WidthTTF(fonts[f], utf8, (unsigned int)px);
}

struct fit_ctx {
    ui_font f;
    int px;
};

static float ttf_width_prefix(const char *s, size_t cp, void *ctx)
{
    struct fit_ctx *fc = (struct fit_ctx *)ctx;
    if (cp == 0) return 0.0f;
    const unsigned char *p = (const unsigned char *)s;
    size_t seen = 0;
    while (*p && seen < cp) {
        p++;
        while (*p && (*p & 0xC0) == 0x80) p++;
        seen++;
    }
    size_t byte_len = (size_t)((const char *)p - s);
    char buf[256];
    size_t n = byte_len < sizeof buf - 1 ? byte_len : sizeof buf - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return (float)GRRLIB_WidthTTF(fonts[fc->f], buf, (unsigned int)fc->px);
}

void ui_text_fit(ui_font f, int px, const char *utf8, float max_w, char *out, size_t out_size)
{
    struct fit_ctx fc = { f, px };
    tf_fit(utf8, max_w, ttf_width_prefix, &fc, out, out_size);
}
