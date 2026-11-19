#include "ldqueue.h"

void lq_want(lq_state *q, lq_job job)
{
    if (job == LQ_LOAD) q->load = true;
    if (job == LQ_CHECK) q->check = true;
    if (job == LQ_TEST) q->test = true;
}

void lq_reset(lq_state *q)
{
    q->load = false;
    q->check = false;
    if (q->owner != LQ_TEST) q->owner = LQ_NONE;
}

void lq_drop(lq_state *q, lq_job job)
{
    if (job == LQ_LOAD) q->load = false;
    if (job == LQ_CHECK) q->check = false;
    if (job == LQ_TEST) q->test = false;
    if (q->owner == job) q->owner = LQ_NONE;
}

lq_job lq_start(lq_state *q, bool busy, bool probe_due)
{
    if (busy || q->owner != LQ_NONE) return LQ_NONE;
    if (q->load) {
        q->load = false;
        q->check = false;
        q->owner = LQ_LOAD;
    } else if (q->test) {
        q->test = false;
        q->owner = LQ_TEST;
    } else if (q->check) {
        q->check = false;
        q->owner = LQ_CHECK;
    } else if (probe_due) {
        q->owner = LQ_PROBE;
    }
    return q->owner;
}

lq_job lq_finish(lq_state *q)
{
    lq_job owner = q->owner;
    q->owner = LQ_NONE;
    return owner;
}

bool lq_new_day(uint32_t *seen, uint32_t day)
{
    bool changed = *seen != 0 && day != 0 && day != *seen;
    *seen = day;
    return changed;
}
