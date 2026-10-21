#include "tween.h"

float tw_ease_out(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

tween tw_make(float from, float to, uint32_t now_ms, uint32_t dur_ms)
{
    tween t;
    t.from = from;
    t.to = to;
    t.start_ms = now_ms;
    t.dur_ms = dur_ms;
    return t;
}

float tw_value(const tween *t, uint32_t now_ms)
{
    if (t->dur_ms == 0) return t->to;
    uint32_t elapsed = now_ms - t->start_ms;
    float raw = t->dur_ms == 0 ? 1.0f : (float)elapsed / (float)t->dur_ms;
    float eased = tw_ease_out(raw);
    return t->from + (t->to - t->from) * eased;
}

bool tw_done(const tween *t, uint32_t now_ms)
{
    return now_ms - t->start_ms >= t->dur_ms;
}
