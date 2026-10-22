#ifndef CTW_MEDIA_AUDIOBUF_H
#define CTW_MEDIA_AUDIOBUF_H

#include <stdint.h>

#define AB_BUFS 2

typedef struct {
    int fill;
} ab_state;

int ab_start(ab_state *s);
int ab_take(ab_state *s);
uint32_t ab_drain(uint32_t *out, uint32_t chunk, const uint32_t *ring, uint32_t ring_size, uint32_t *r_pos, uint32_t w_pos);

#endif
