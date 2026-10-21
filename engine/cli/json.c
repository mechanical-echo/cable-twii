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
            } else {
                fputc(c, f);
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
