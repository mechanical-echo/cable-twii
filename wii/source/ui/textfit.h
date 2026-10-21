#ifndef CTW_UI_TEXTFIT_H
#define CTW_UI_TEXTFIT_H

#include <stddef.h>

typedef float (*tf_width_fn)(const char *s, size_t len, void *ctx);

size_t tf_fit(const char *utf8, float max_w, tf_width_fn width, void *ctx, char *out, size_t out_size);

#endif
