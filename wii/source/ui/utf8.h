#ifndef CTW_UI_UTF8_H
#define CTW_UI_UTF8_H

#include <stddef.h>
#include <stdint.h>

#define U8_REPLACEMENT 0xFFFDu

size_t u8_decode(const char *s, size_t len, uint32_t *out, size_t cap);
size_t u8_prefix_bytes(const char *s, size_t cp);

#endif
