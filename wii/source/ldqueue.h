#ifndef CTW_LDQUEUE_H
#define CTW_LDQUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { LQ_NONE, LQ_LOAD, LQ_CHECK, LQ_PROBE } lq_job;

typedef struct {
    bool load;
    bool check;
    lq_job owner;
} lq_state;

void lq_want(lq_state *q, lq_job job);
void lq_reset(lq_state *q);
lq_job lq_start(lq_state *q, bool busy, bool probe_due);
lq_job lq_finish(lq_state *q);
bool lq_new_day(uint32_t *seen, uint32_t day);

#endif
