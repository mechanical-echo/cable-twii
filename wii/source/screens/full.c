#include "full.h"

#include <stdio.h>
#include <string.h>
#include <wiiuse/wpad.h>

#include "../input.h"
#include "../nav.h"
#include "../osd.h"
#include "../osd_layout.h"
#include "../live.h"
#include "../clock.h"
#include "../guide_text.h"
#include "../media/player.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/press.h"
#include "../ui/text.h"
#include "../ui/cursor.h"
#include "../ui/sfx.h"
#include "../ui/theme.h"
#include "panel.h"
#include "upnext.h"

#define CHIP_TILE_RADIUS 10.7f
#define CHIP_MID 0.5f

static osd_state g_osd;
static float last_cx, last_cy;
static bool have_last_cursor;
static osd_btn hot_btn;
static ps_state press;
static bool chip_valid;
static int chip_channel;
static char chip_number[16];
static float chip_number_w;
static char chip_name[TF_LINE_BYTES];
static float chip_name_w;

static void back_to_guide(app *a)
{
    sfx_play(SFX_BACK);
    app_go(a, SCR_GUIDE);
}

static void flip(app *a, int dir)
{
    tune(a, nav_step(a->db, a->current_channel, dir));
}

static void change_volume(app *a, int dir)
{
    a->volume = osd_volume_step(a->volume, dir);
    pl_set_volume(a->volume);
    osd_volume_changed(&g_osd, a->now_ms);
}

static float slide_offset(const app *a)
{
    osd_layout rest = osd_layout_for(0.0f, ui_pixel_x(), 0.0f, 0.0f, 0.0f, 0.0f);
    return (1.0f - osd_slide(&g_osd, a->now_ms)) * (480.0f - rest.panel.y);
}

static void panel_input(app *a)
{
    bool volume = osd_volume_visible(&g_osd, a->now_ms);
    osd_layout l = panel_layout(a, slide_offset(a));
    osd_btn btn = cur_visible() && volume ? osd_hit(&l, cur_x(), cur_y(), true) : OSD_BTN_NONE;
    if (btn != OSD_BTN_NONE && btn != hot_btn) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot_btn = btn;
    int fired = ps_update(&press, (int)btn, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired == OSD_BTN_VOL_DOWN) {
        sfx_play(SFX_SELECT);
        change_volume(a, -1);
    } else if (fired == OSD_BTN_VOL_UP) {
        sfx_play(SFX_SELECT);
        change_volume(a, 1);
    }
}

void tune(app *a, int channel)
{
    sfx_play(SFX_CHANNEL);
    osd_channel_changed(&g_osd, a->now_ms);
    live_tune(a, channel);
}

void full_enter(app *a)
{
    have_last_cursor = false;
    memset(&g_osd, 0, sizeof g_osd);
    memset(&press, 0, sizeof press);
    hot_btn = OSD_BTN_NONE;
    osd_channel_changed(&g_osd, a->now_ms);
    if (!live_on(a->current_channel)) live_tune(a, a->current_channel);
}

void full_update(app *a)
{
    clk_now(&a->today, &a->ms_of_day);

    if (in_pressed(WPAD_BUTTON_B)) {
        back_to_guide(a);
        return;
    }

    if (in_pressed(WPAD_BUTTON_UP) || in_pressed(WPAD_BUTTON_RIGHT)) {
        flip(a, 1);
    } else if (in_pressed(WPAD_BUTTON_DOWN) || in_pressed(WPAD_BUTTON_LEFT)) {
        flip(a, -1);
    }

    if (in_pressed(WPAD_BUTTON_PLUS)) change_volume(a, 1);
    if (in_pressed(WPAD_BUTTON_MINUS)) change_volume(a, -1);

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

    panel_input(a);
    if (a->screen != SCR_FULL) return;

    live_update(a);
}

bool full_cursor_visible(const app *a)
{
    return osd_visible(&g_osd, a->now_ms) || osd_volume_visible(&g_osd, a->now_ms);
}

static void chip_refresh(app *a)
{
    if (chip_valid && chip_channel == a->current_channel) return;
    char num[16];
    gt_number(a->current_channel, num, sizeof num);
    ui_text_fit(FONT_BOLD, OSD_CHIP_NUMBER_PX, num, OSD_CHIP_NUMBER_MAX_W, chip_number, sizeof chip_number);
    chip_number_w = ui_text_width(FONT_BOLD, OSD_CHIP_NUMBER_PX, chip_number);
    const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
    ui_text_fit(FONT_BOLD, OSD_CHIP_NAME_PX, ch != NULL ? ch->name : "", OSD_CHIP_NAME_MAX_W, chip_name, sizeof chip_name);
    chip_name_w = ui_text_width(FONT_BOLD, OSD_CHIP_NAME_PX, chip_name);
    chip_channel = a->current_channel;
    chip_valid = true;
}

static void chip_draw(app *a)
{
    chip_refresh(a);
    osd_chip c = osd_chip_layout(chip_number_w, chip_name_w, ui_pixel_x());
    float drop = osd_chip_name_drop(&g_osd, a->now_ms);
    float hidden_y = c.chip.y + c.chip.h - c.name_pill.h;
    float pill_y = hidden_y + (c.name_pill.y - hidden_y) * drop;
    ui_pill_ex(c.name_pill.x, pill_y, c.name_pill.w, c.name_pill.h, chip_name, OSD_CHIP_NAME_PX, UI_PILL_REST, UI_SHADOW_VIDEO);

    float glow = osd_chip_glow(&g_osd, a->now_ms);
    float radius = c.chip.h / 2.0f;
    if (glow > 0.0f) ui_ring(c.chip.x, c.chip.y, c.chip.w, c.chip.h, radius, (THEME_RING & 0xFFFFFF00u) | (uint32_t)((THEME_RING & 0xFFu) * glow), THEME_RING_SPREAD);
    ui_card(c.chip.x, c.chip.y, c.chip.w, c.chip.h, radius, CHIP_MID, THEME_GLOSS_COOL, UI_SHADOW_VIDEO);
    ui_accent_tile(c.tile.x, c.tile.y, c.tile.w, c.tile.h, CHIP_TILE_RADIUS);
    ui_text(FONT_BOLD, OSD_CHIP_NUMBER_PX, c.tile.x + c.tile.w / 2.0f, c.tile.y + (c.tile.h - (float)OSD_CHIP_NUMBER_PX) / 2.0f, chip_number, THEME_PANEL_FILL, ALIGN_CENTER);
    const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
    char path[320] = "";
    if (ch != NULL) gt_icon_path(a->root, ch, NULL, path, sizeof path);
    icons_draw(path, c.icon.x - (c.icon.h - c.icon.w) / 2.0f, c.icon.y, c.icon.h);
}

void full_draw(app *a)
{
    icons_frame();
    pl_draw(0, 0, 640, 480, 1.0f);

    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, a->current_channel, a->today, a->ms_of_day, &offset);
    if (air != NULL && air->kind == CTW_AIR_UPNEXT) {
        const ctw_airing *next = tn_next(&a->tuner, a->current_channel, air);
        const ctw_airing *ep = tn_next_episode(&a->tuner, a->current_channel, air);
        int64_t until = ep != NULL ? tn_ms_until(&a->tuner, a->current_channel, air, offset, ep) : (int64_t)tn_ms_remaining(air, offset);
        upnext_draw(a, air, offset, next, ep, until);
    }

    if (osd_chip_visible(&g_osd, a->now_ms)) chip_draw(a);

    if (osd_panel_visible(&g_osd, a->now_ms)) {
        osd_layout l = panel_layout(a, slide_offset(a));
        panel_draw(a, &l);
    } else if (osd_volume_visible(&g_osd, a->now_ms)) {
        osd_layout l = panel_layout(a, 0.0f);
        panel_draw_volume(&l, a->volume, hot_btn, (osd_btn)ps_pressed(&press, (int)hot_btn));
    }
}
