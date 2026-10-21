#include "ctw/ctw.h"

#define CTW_PCG_MULT 6364136223846793005ULL

uint32_t ctw_rng_next(ctw_rng *r)
{
    uint64_t old = r->state;
    r->state = old * CTW_PCG_MULT + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

ctw_rng ctw_rng_seed(uint64_t initstate, uint64_t initseq)
{
    ctw_rng r;
    r.state = 0u;
    r.inc = (initseq << 1u) | 1u;
    ctw_rng_next(&r);
    r.state += initstate;
    ctw_rng_next(&r);
    return r;
}

uint32_t ctw_rng_below(ctw_rng *r, uint32_t bound)
{
    if (bound == 0u) {
        return 0u;
    }
    uint32_t threshold = (0u - bound) % bound;
    for (;;) {
        uint32_t v = ctw_rng_next(r);
        if (v >= threshold) {
            return v % bound;
        }
    }
}

void ctw_shuffle(ctw_rng *r, uint32_t *items, size_t n)
{
    for (size_t i = n; i > 1; i--) {
        size_t j = ctw_rng_below(r, (uint32_t)i);
        uint32_t tmp = items[i - 1];
        items[i - 1] = items[j];
        items[j] = tmp;
    }
}
