#ifndef CTW_MEDIA_NETRING_H
#define CTW_MEDIA_NETRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NR_KEEP_BEHIND (256u * 1024u)

typedef struct {
    uint8_t *mem;
    size_t cap;
    uint64_t start;
    size_t len;
} nr_ring;

void nr_init(nr_ring *r, uint8_t *mem, size_t cap);
void nr_reset(nr_ring *r, uint64_t pos);
bool nr_contains(const nr_ring *r, uint64_t pos);
uint64_t nr_end(const nr_ring *r);
size_t nr_space(const nr_ring *r);
void nr_append(nr_ring *r, const uint8_t *src, size_t n);
size_t nr_copy(const nr_ring *r, uint64_t pos, uint8_t *dst, size_t n);
void nr_drop_before(nr_ring *r, uint64_t pos);

#endif
