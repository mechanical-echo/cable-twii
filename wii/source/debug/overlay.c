#include "overlay.h"

#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>

#include "dbgstats.h"
#include "../media/audio.h"
#include "../media/player.h"
#include "../ui/draw.h"
#include "../ui/text.h"

#define OVERLAY_X 8.0f
#define OVERLAY_Y 8.0f
#define OVERLAY_W 236.0f
#define OVERLAY_PAD 6.0f
#define OVERLAY_LINE_H 15.0f
#define OVERLAY_TEXT_PX 12
#define OVERLAY_FILL 0x000000B0u
#define OVERLAY_TEXT 0xFFFFFFFFu

static dbg_rate ui_rate, video_rate;
static uint32_t frames;
static char lines[DBG_LINES][DBG_LINE_BYTES];

static void refresh(void)
{
    dbg_stats s;
    memset(&s, 0, sizeof s);
    s.ui_fps = ui_rate.rate;
    s.video_fps = video_rate.rate;
    s.underruns = au_underrun_count();
    uint32_t shown;
    pl_debug_counts(&s.decoded, &shown, &s.dropped);
    s.mem1_free = SYS_GetArena1Size();
    s.mem2_free = SYS_GetArena2Size();
    struct mallinfo mi = mallinfo();
    s.heap_used = (uint32_t)mi.uordblks;
    s.heap_free = (uint32_t)mi.fordblks;
    dbg_format(&s, lines);
}

void overlay_init(void)
{
    dbg_rate_init(&ui_rate);
    dbg_rate_init(&video_rate);
    frames = 0;
    refresh();
}

void overlay_draw(void)
{
    frames++;
    uint64_t now_us = ticks_to_microsecs(gettime());
    uint32_t decoded, shown, dropped;
    pl_debug_counts(&decoded, &shown, &dropped);
    dbg_rate_update(&video_rate, now_us, shown);
    if (dbg_rate_update(&ui_rate, now_us, frames)) refresh();

    float h = 2.0f * OVERLAY_PAD + DBG_LINES * OVERLAY_LINE_H;
    ui_rounded(OVERLAY_X, OVERLAY_Y, OVERLAY_W, h, 6.0f, OVERLAY_FILL);
    for (int i = 0; i < DBG_LINES; i++) {
        float y = OVERLAY_Y + OVERLAY_PAD + (float)i * OVERLAY_LINE_H;
        ui_text(FONT_BODY, OVERLAY_TEXT_PX, OVERLAY_X + OVERLAY_PAD, y, lines[i], OVERLAY_TEXT, ALIGN_LEFT);
    }
}
