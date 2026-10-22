#include "audiobuf.h"

#include <string.h>

int ab_start(ab_state *s)
{
    s->fill = 1 % AB_BUFS;
    return 0;
}

int ab_take(ab_state *s)
{
    int idx = s->fill;
    s->fill = (s->fill + 1) % AB_BUFS;
    return idx;
}

uint32_t ab_drain(uint32_t *out, uint32_t chunk, const uint32_t *ring, uint32_t ring_size, uint32_t *r_pos, uint32_t w_pos)
{
    uint32_t r = *r_pos;
    uint32_t avail = w_pos - r;
    uint32_t n = avail < chunk ? avail : chunk;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = ring[(r + i) & (ring_size - 1)];
    }
    if (n < chunk) memset(out + n, 0, (size_t)(chunk - n) * sizeof out[0]);
    *r_pos = r + n;
    return n;
}
