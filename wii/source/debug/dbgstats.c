#include "dbgstats.h"

#include <stdio.h>
#include <string.h>

void dbg_rate_init(dbg_rate *r)
{
    memset(r, 0, sizeof *r);
}

bool dbg_rate_update(dbg_rate *r, uint64_t now_us, uint32_t count)
{
    if (!r->started) {
        r->started = true;
        r->start_us = now_us;
        r->start_count = count;
        return false;
    }
    uint64_t elapsed = now_us - r->start_us;
    if (elapsed < DBG_RATE_WINDOW_US) return false;
    uint32_t frames = count - r->start_count;
    r->rate = (float)((double)frames * 1000000.0 / (double)elapsed);
    r->start_us = now_us;
    r->start_count = count;
    return true;
}

static unsigned int kilobytes(uint32_t bytes)
{
    return (unsigned int)(bytes / 1024u);
}

void dbg_format(const dbg_stats *s, char lines[DBG_LINES][DBG_LINE_BYTES])
{
    snprintf(lines[0], DBG_LINE_BYTES, "UI %.1f fps  video %.1f fps", (double)s->ui_fps, (double)s->video_fps);
    snprintf(lines[1], DBG_LINE_BYTES, "audio underruns %u", (unsigned int)s->underruns);
    snprintf(lines[2], DBG_LINE_BYTES, "frames %u decoded  %u dropped", (unsigned int)s->decoded, (unsigned int)s->dropped);
    snprintf(lines[3], DBG_LINE_BYTES, "MEM1 %uK  MEM2 %uK free", kilobytes(s->mem1_free), kilobytes(s->mem2_free));
    snprintf(lines[4], DBG_LINE_BYTES, "heap %uK used  %uK free", kilobytes(s->heap_used), kilobytes(s->heap_free));
}
