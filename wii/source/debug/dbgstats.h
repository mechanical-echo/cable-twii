#ifndef CTW_DEBUG_DBGSTATS_H
#define CTW_DEBUG_DBGSTATS_H

#include <stdbool.h>
#include <stdint.h>

#define DBG_RATE_WINDOW_US 1000000u
#define DBG_LINES 5
#define DBG_LINE_BYTES 64

typedef struct {
    uint64_t start_us;
    uint32_t start_count;
    float rate;
    bool started;
} dbg_rate;

typedef struct {
    float ui_fps;
    float video_fps;
    uint32_t underruns;
    uint32_t decoded;
    uint32_t dropped;
    uint32_t mem1_free;
    uint32_t mem2_free;
    uint32_t heap_used;
    uint32_t heap_free;
} dbg_stats;

void dbg_rate_init(dbg_rate *r);
bool dbg_rate_update(dbg_rate *r, uint64_t now_us, uint32_t count);
void dbg_format(const dbg_stats *s, char lines[DBG_LINES][DBG_LINE_BYTES]);

#endif
