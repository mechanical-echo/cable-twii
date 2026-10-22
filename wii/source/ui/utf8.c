#include "utf8.h"

static size_t decode_one(const unsigned char *p, size_t len, uint32_t *cp)
{
    unsigned char b = p[0];
    if (b < 0x80) {
        *cp = b;
        return 1;
    }

    size_t need;
    uint32_t value;
    unsigned char lo = 0x80, hi = 0xBF;
    if (b >= 0xC2 && b <= 0xDF) {
        need = 1;
        value = b & 0x1Fu;
    } else if (b >= 0xE0 && b <= 0xEF) {
        need = 2;
        value = b & 0x0Fu;
        if (b == 0xE0) lo = 0xA0;
        if (b == 0xED) hi = 0x9F;
    } else if (b >= 0xF0 && b <= 0xF4) {
        need = 3;
        value = b & 0x07u;
        if (b == 0xF0) lo = 0x90;
        if (b == 0xF4) hi = 0x8F;
    } else {
        *cp = U8_REPLACEMENT;
        return 1;
    }

    size_t used = 1;
    for (size_t i = 0; i < need; i++) {
        if (used >= len) {
            *cp = U8_REPLACEMENT;
            return used;
        }
        unsigned char c = p[used];
        unsigned char min = i == 0 ? lo : 0x80;
        unsigned char max = i == 0 ? hi : 0xBF;
        if (c < min || c > max) {
            *cp = U8_REPLACEMENT;
            return used;
        }
        value = (value << 6) | (c & 0x3Fu);
        used++;
    }
    *cp = value;
    return used;
}

size_t u8_decode(const char *s, size_t len, uint32_t *out, size_t cap)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t pos = 0, n = 0;
    while (pos < len && p[pos] != '\0' && n < cap) {
        pos += decode_one(p + pos, len - pos, &out[n]);
        n++;
    }
    return n;
}

size_t u8_prefix_bytes(const char *s, size_t cp)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t len = 0;
    while (p[len] != '\0') len++;
    size_t pos = 0, seen = 0;
    while (pos < len && seen < cp) {
        uint32_t ignored;
        pos += decode_one(p + pos, len - pos, &ignored);
        seen++;
    }
    return pos;
}
