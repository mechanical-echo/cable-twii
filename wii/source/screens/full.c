#include "full.h"

#include <stdio.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "../input.h"
#include "../nav.h"
#include "../osd.h"
#include "../playback.h"
#include "../storage.h"
#include "../clock.h"
#include "../media/player.h"
#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/cursor.h"
#include "../ui/sfx.h"
#include "../ui/theme.h"
#include "upnext.h"

static osd_state g_osd;
static int64_t current_start_ms = -1;
static float last_cx, last_cy;
static bool have_last_cursor;

static void play_airing(app *a, const ctw_airing *air, uint32_t offset_ms)
{
    if (air == NULL || air->kind == CTW_AIR_UPNEXT) {
        pl_close();
        current_start_ms = air != NULL ? tn_airing_start(&a->tuner, a->current_channel, air) : -1;
        return;
    }
    char path[320];
    const char *rel = air->kind == CTW_AIR_AD ? air->ad->path : air->episode->path;
    snprintf(path, sizeof path, "%s/%s", a->root, rel);
    pl_open(path, offset_ms);
    current_start_ms = tn_airing_start(&a->tuner, a->current_channel, air);
}

void tune(app *a, int channel)
{
    a->current_channel = channel;
    sfx_play(SFX_CHANNEL);
    osd_channel_changed(&g_osd, a->now_ms);
    clk_now(&a->today, &a->ms_of_day);
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, channel, a->today, a->ms_of_day, &offset);
    play_airing(a, air, offset);
}

void full_enter(app *a)
{
    current_start_ms = -1;
    have_last_cursor = false;
    tune(a, a->current_channel);
}

void full_update(app *a)
{
    clk_now(&a->today, &a->ms_of_day);

    pl_state st = pl_status();
    if (st == PL_ERROR) pl_close();
    if (pb_on_status(st, st == PL_ERROR && st_drive_present(&a->drive)) == PB_DISCONNECTED) {
        app_fail(a, "The drive was disconnected", "Plug it back in and start cable-twii again.");
        return;
    }

    if (in_pressed(WPAD_BUTTON_UP) || in_pressed(WPAD_BUTTON_RIGHT)) {
        tune(a, nav_step(a->db, a->current_channel, 1));
    } else if (in_pressed(WPAD_BUTTON_DOWN) || in_pressed(WPAD_BUTTON_LEFT)) {
        tune(a, nav_step(a->db, a->current_channel, -1));
    }
    if (in_pressed(WPAD_BUTTON_B)) {
        pl_close();
        app_go(a, SCR_LOGO);
        return;
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

    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, a->current_channel, a->today, a->ms_of_day, &offset);
    if (air != NULL && tn_airing_start(&a->tuner, a->current_channel, air) != current_start_ms) {
        play_airing(a, air, offset);
    }
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
