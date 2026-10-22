#include "textfit.h"
#include <string.h>

static int is_cont_byte(unsigned char b)
{
    return (b & 0xC0) == 0x80;
}

static size_t cp_count(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (!is_cont_byte(*p)) n++;
    }
    return n;
}

static size_t byte_len_for_cp(const char *s, size_t cp)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t seen = 0;
    while (*p && seen < cp) {
        p++;
        while (*p && is_cont_byte(*p)) p++;
        seen++;
    }
    return (size_t)((const char *)p - s);
}

static const char ELLIPSIS[] = "\xE2\x80\xA6";

size_t tf_fit(const char *utf8, float max_w, tf_width_fn width, void *ctx, char *out, size_t out_size)
{
    if (out_size == 0) return 0;

    size_t total_cp = cp_count(utf8);
    size_t total_bytes = strlen(utf8);

    if (width(utf8, total_cp, ctx) <= max_w) {
        size_t n = total_bytes < out_size - 1 ? total_bytes : out_size - 1;
        memcpy(out, utf8, n);
        out[n] = '\0';
        return n;
    }

    float ellipsis_w = width(ELLIPSIS, 1, ctx);
    size_t ellipsis_bytes = sizeof(ELLIPSIS) - 1;

    for (size_t k = total_cp; k-- > 0; ) {
        if (width(utf8, k, ctx) + ellipsis_w >= max_w) continue;

        size_t prefix_bytes = byte_len_for_cp(utf8, k);
        size_t need = prefix_bytes + ellipsis_bytes;
        size_t n = need < out_size - 1 ? need : out_size - 1;
        size_t copy_prefix = prefix_bytes < n ? prefix_bytes : n;
        memcpy(out, utf8, copy_prefix);
        size_t remain = n - copy_prefix;
        size_t copy_ellipsis = remain < ellipsis_bytes ? remain : ellipsis_bytes;
        memcpy(out + copy_prefix, ELLIPSIS, copy_ellipsis);
        out[copy_prefix + copy_ellipsis] = '\0';
        return copy_prefix + copy_ellipsis;
    }

    size_t n = ellipsis_bytes < out_size - 1 ? ellipsis_bytes : out_size - 1;
    memcpy(out, ELLIPSIS, n);
    out[n] = '\0';
    return n;
}

static size_t para_cp_count(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && *p != '\n'; p++) {
        if (!is_cont_byte(*p)) n++;
    }
    return n;
}

static void set_line(tf_line *line, const char *s, size_t bytes)
{
    while (bytes > 0 && s[bytes - 1] == ' ') bytes--;
    memcpy(line->text, s, bytes);
    line->text[bytes] = '\0';
}

static void ellipsize_line(tf_line *line, float max_w, tf_width_fn width, void *ctx)
{
    float ellipsis_w = width(ELLIPSIS, 1, ctx);
    size_t k = cp_count(line->text);
    while (k > 0 && width(line->text, k, ctx) + ellipsis_w > max_w) k--;
    size_t bytes = byte_len_for_cp(line->text, k);
    while (bytes > 0 && line->text[bytes - 1] == ' ') bytes--;
    memcpy(line->text + bytes, ELLIPSIS, sizeof(ELLIPSIS));
}

size_t tf_wrap(const char *utf8, float max_w, size_t max_lines, tf_width_fn width, void *ctx, tf_line *lines)
{
    const size_t max_bytes = TF_LINE_BYTES - sizeof(ELLIPSIS);
    const char *p = utf8;
    size_t count = 0;

    while (*p != '\0' && count < max_lines) {
        size_t para = para_cp_count(p);
        size_t k = 0;
        while (k < para && byte_len_for_cp(p, k + 1) <= max_bytes && width(p, k + 1, ctx) <= max_w) k++;

        size_t line_bytes;
        const char *next;
        if (k == para) {
            line_bytes = byte_len_for_cp(p, k);
            next = p + line_bytes;
            if (*next == '\n') next++;
        } else {
            if (k == 0) k = 1;
            size_t brk = 0;
            for (size_t j = k; j > 0; j--) {
                if (p[byte_len_for_cp(p, j)] == ' ') {
                    brk = j;
                    break;
                }
            }
            if (brk > 0) {
                line_bytes = byte_len_for_cp(p, brk);
                next = p + line_bytes;
                while (*next == ' ') next++;
                if (*next == '\n') next++;
            } else {
                line_bytes = byte_len_for_cp(p, k);
                next = p + line_bytes;
            }
        }

        set_line(&lines[count], p, line_bytes);
        count++;
        p = next;
        if (count == max_lines && *p != '\0') ellipsize_line(&lines[count - 1], max_w, width, ctx);
    }
    return count;
}
