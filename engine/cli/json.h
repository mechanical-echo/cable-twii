#ifndef CTW_JSON_H
#define CTW_JSON_H

#include <stdio.h>

#define JSON_MAX_DEPTH 16

typedef struct {
    FILE *f;
    int depth;
    int has_item[JSON_MAX_DEPTH];
    int after_key;
} json_w;

void json_init(json_w *w, FILE *f);
void json_begin_obj(json_w *w);
void json_end_obj(json_w *w);
void json_begin_arr(json_w *w);
void json_end_arr(json_w *w);
void json_key(json_w *w, const char *key);
void json_str(json_w *w, const char *s);
void json_int(json_w *w, long long v);
void json_bool(json_w *w, int v);
void json_null(json_w *w);

#endif
