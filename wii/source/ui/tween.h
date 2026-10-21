#ifndef CTW_UI_TWEEN_H
#define CTW_UI_TWEEN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float from;
    float to;
    uint32_t start_ms;
    uint32_t dur_ms;
} tween;

float tw_ease_out(float t);
tween tw_make(float from, float to, uint32_t now_ms, uint32_t dur_ms);
float tw_value(const tween *t, uint32_t now_ms);
bool tw_done(const tween *t, uint32_t now_ms);

#endif
