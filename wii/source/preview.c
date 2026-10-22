#include "preview.h"

void pv_reset(pv_state *s, int tuned)
{
    s->pending = tuned;
    s->since_ms = 0;
    s->tuned = tuned;
}

int pv_update(pv_state *s, int highlighted, uint32_t now_ms)
{
    if (highlighted != s->pending) {
        s->pending = highlighted;
        s->since_ms = now_ms;
    }
    if (s->pending == 0 || s->pending == s->tuned) return 0;
    if (now_ms - s->since_ms < PV_DEBOUNCE_MS) return 0;
    s->tuned = s->pending;
    return s->tuned;
}

void pv_force(pv_state *s, int channel)
{
    s->pending = channel;
    s->tuned = channel;
}

int pv_tuned(const pv_state *s)
{
    return s->tuned;
}
