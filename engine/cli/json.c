#include "json.h"

static void before_value(json_w *w)
{
    if (w->after_key) {
        w->after_key = 0;
        return;
    }
    if (w->depth > 0) {
        if (w->has_item[w->depth - 1]) fputc(',', w->f);
        w->has_item[w->depth - 1] = 1;
    }
}

static int utf8_len(const unsigned char *s)
{
    unsigned char c = s[0];
    if (c >= 0xC2 && c <= 0xDF) {
        return (s[1] & 0xC0) == 0x80 ? 2 : 0;
    }
    if (c >= 0xE0 && c <= 0xEF) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return 0;
        if (c == 0xE0 && s[1] < 0xA0) return 0;
        if (c == 0xED && s[1] > 0x9F) return 0;
        return 3;
    }
    if (c >= 0xF0 && c <= 0xF4) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return 0;
        if (c == 0xF0 && s[1] < 0x90) return 0;
        if (c == 0xF4 && s[1] > 0x8F) return 0;
        return 4;
    }
    return 0;
}

static void write_escaped(FILE *f, const char *s)
{
    static const char hex[] = "0123456789abcdef";
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"': fputs("\\\"", f); break;
        case '\\': fputs("\\\\", f); break;
        case '\b': fputs("\\b", f); break;
        case '\f': fputs("\\f", f); break;
        case '\n': fputs("\\n", f); break;
        case '\r': fputs("\\r", f); break;
        case '\t': fputs("\\t", f); break;
        default:
            if (c < 0x20) {
                fputs("\\u00", f);
                fputc(hex[c >> 4], f);
                fputc(hex[c & 15], f);
            } else if (c < 0x80) {
                fputc(c, f);
            } else {
                int n = utf8_len((const unsigned char *)s);
                if (n == 0) {
                    fputs("\xEF\xBF\xBD", f);
                } else {
                    fwrite(s, 1, (size_t)n, f);
                    s += n - 1;
                }
            }
        }
    }
    fputc('"', f);
}

void json_init(json_w *w, FILE *f)
{
    w->f = f;
    w->depth = 0;
    w->after_key = 0;
    for (int i = 0; i < JSON_MAX_DEPTH; i++) w->has_item[i] = 0;
}

static void begin(json_w *w, char open)
{
    before_value(w);
    fputc(open, w->f);
    if (w->depth < JSON_MAX_DEPTH) w->has_item[w->depth] = 0;
    w->depth++;
}

static void end(json_w *w, char close)
{
    if (w->depth > 0) w->depth--;
    fputc(close, w->f);
}

void json_begin_obj(json_w *w) { begin(w, '{'); }
void json_end_obj(json_w *w) { end(w, '}'); }
void json_begin_arr(json_w *w) { begin(w, '['); }
void json_end_arr(json_w *w) { end(w, ']'); }

void json_key(json_w *w, const char *key)
{
    before_value(w);
    write_escaped(w->f, key);
    fputc(':', w->f);
    w->after_key = 1;
}

void json_str(json_w *w, const char *s)
{
    if (!s) {
        json_null(w);
        return;
    }
    before_value(w);
    write_escaped(w->f, s);
}

void json_int(json_w *w, long long v)
{
    before_value(w);
    fprintf(w->f, "%lld", v);
}

void json_bool(json_w *w, int v)
{
    before_value(w);
    fputs(v ? "true" : "false", w->f);
}

void json_null(json_w *w)
{
    before_value(w);
    fputs("null", w->f);
}
