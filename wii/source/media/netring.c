#include "netring.h"

#include <string.h>

static size_t slot(const nr_ring *r, uint64_t pos)
{
    return (size_t)(pos % r->cap);
}

void nr_init(nr_ring *r, uint8_t *mem, size_t cap)
{
    r->mem = mem;
    r->cap = cap;
    nr_reset(r, 0);
}

void nr_reset(nr_ring *r, uint64_t pos)
{
    r->start = pos;
    r->len = 0;
}

bool nr_contains(const nr_ring *r, uint64_t pos)
{
    return pos >= r->start && pos - r->start < r->len;
}

uint64_t nr_end(const nr_ring *r)
{
    return r->start + r->len;
}

size_t nr_space(const nr_ring *r)
{
    return r->cap - r->len;
}

void nr_append(nr_ring *r, const uint8_t *src, size_t n)
{
    if (n > r->cap) {
        r->start += r->len + (n - r->cap);
        r->len = 0;
        src += n - r->cap;
        n = r->cap;
    }
    if (n > nr_space(r)) {
        size_t over = n - nr_space(r);
        r->start += over;
        r->len -= over;
    }
    size_t at = slot(r, nr_end(r));
    size_t first = r->cap - at < n ? r->cap - at : n;
    memcpy(r->mem + at, src, first);
    memcpy(r->mem, src + first, n - first);
    r->len += n;
}

size_t nr_copy(const nr_ring *r, uint64_t pos, uint8_t *dst, size_t n)
{
    if (!nr_contains(r, pos)) return 0;
    uint64_t left = nr_end(r) - pos;
    if (n > left) n = (size_t)left;
    size_t at = slot(r, pos);
    size_t first = r->cap - at < n ? r->cap - at : n;
    memcpy(dst, r->mem + at, first);
    memcpy(dst + first, r->mem, n - first);
    return n;
}

void nr_drop_before(nr_ring *r, uint64_t pos)
{
    if (pos < r->start || pos - r->start <= NR_KEEP_BEHIND) return;
    uint64_t keep = pos - NR_KEEP_BEHIND;
    uint64_t end = nr_end(r);
    if (keep > end) keep = end;
    r->len -= (size_t)(keep - r->start);
    r->start = keep;
}
