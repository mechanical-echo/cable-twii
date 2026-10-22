#include "full.h"

#include <stdio.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "../input.h"
#include "../nav.h"
#include "../osd.h"
#include "../live.h"
#include "../clock.h"
#include "../media/player.h"
#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/cursor.h"
#include "../ui/sfx.h"
#include "../ui/theme.h"
#include "upnext.h"

static osd_state g_osd;
static float last_cx, last_cy;
static bool have_last_cursor;

void tune(app *a, int channel)
{
    sfx_play(SFX_CHANNEL);
    osd_channel_changed(&g_osd, a->now_ms);
    live_tune(a, channel);
}

void full_enter(app *a)
{
    have_last_cursor = false;
    osd_channel_changed(&g_osd, a->now_ms);
    if (!live_on(a->current_channel)) live_tune(a, a->current_channel);
}

void full_update(app *a)
{
    clk_now(&a->today, &a->ms_of_day);

    if (in_pressed(WPAD_BUTTON_B)) {
        sfx_play(SFX_BACK);
        app_go(a, SCR_GUIDE);
        return;
    }

    if (in_pressed(WPAD_BUTTON_UP) || in_pressed(WPAD_BUTTON_RIGHT)) {
        tune(a, nav_step(a->db, a->current_channel, 1));
    } else if (in_pressed(WPAD_BUTTON_DOWN) || in_pressed(WPAD_BUTTON_LEFT)) {
        tune(a, nav_step(a->db, a->current_channel, -1));
    }

    bool moved = false;
    if (cur_visible()) {
        float cx = cur_x(), cy = cur_y();
        moved = !have_last_cursor || cx != last_cx || cy != last_cy;
        last_cx = cx;
        last_cy = cy;
        have_last_cursor = true;
    } else {
        have_last_cursor = false;
    }
    osd_pointer(&g_osd, cur_visible(), moved, a->now_ms);

    live_update(a);
}

void full_draw(app *a)
{
    pl_draw(0, 0, 640, 480, 1.0f);

    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, a->current_channel, a->today, a->ms_of_day, &offset);
    if (air != NULL && air->kind == CTW_AIR_UPNEXT) {
        const ctw_airing *next = tn_next(&a->tuner, a->current_channel, air);
        uint32_t remaining = tn_ms_remaining(air, offset);
        upnext_draw(a, air, next, remaining);
    }

    if (osd_chip_visible(&g_osd, a->now_ms)) {
        const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
        float chip_w = 120.0f, chip_h = 48.0f;
        float x = 640.0f - chip_w - 16.0f, y = 16.0f;
        ui_panel(x, y, chip_w, chip_h, chip_h / 2.0f, THEME_PILL_FILL_TOP, THEME_PILL_BORDER);
        char num[16];
        snprintf(num, sizeof num, "%d", a->current_channel);
        ui_text(FONT_BOLD, 24, x + chip_w / 2.0f, y + chip_h / 2.0f - 14.0f, num, THEME_ACCENT, ALIGN_CENTER);
        (void)ch;
    }
}
