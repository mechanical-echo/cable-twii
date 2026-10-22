#include "osd_layout.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define MARGIN_X 26.0f
#define MARGIN_BOTTOM 24.0f
#define PANEL_H 108.0f
#define ICON_X 14.0f
#define ICON_Y 30.0f
#define ICON_PX 48.0f
#define TEXT_X 60.7f
#define NAME_Y 22.3f
#define NAME_H 18.0f
#define NUMBER_GAP 5.3f
#define NUMBER_Y 24.3f
#define NUMBER_MIN_W 20.0f
#define NUMBER_H 16.0f
#define LINE_Y 47.2f
#define LINE_H 16.0f
#define TIMES_Y 70.8f
#define TIMES_H 12.4f
#define BAR_GAP 5.9f
#define BAR_Y 74.3f
#define BAR_H 8.0f
#define BAR_RIGHT 352.0f
#define END_RIGHT 385.3f
#define DIVIDER_X 396.0f
#define DIVIDER_Y 16.2f
#define DIVIDER_H 75.6f
#define NEXT_ICON_X 407.3f
#define NEXT_ICON_Y 34.0f
#define NEXT_ICON_PX 40.0f
#define NEXT_TEXT_GAP 8.0f
#define NEXT_LABEL_Y 20.4f
#define NEXT_LABEL_H 13.3f
#define NEXT_TITLE_Y 38.1f
#define NEXT_TITLE_H 32.0f
#define NEXT_AT_Y 73.7f
#define NEXT_AT_H 12.0f
#define NEXT_RIGHT 515.7f
#define COUNTDOWN_RIGHT 574.0f
#define COUNTDOWN_Y 40.0f
#define COUNTDOWN_MIN_W 50.2f
#define COUNTDOWN_GAP 6.0f
#define COUNTDOWN_H 28.0f
#define SEGMENT_GAP 3.0f
#define VOL_PANEL_W 260.0f
#define VOL_PANEL_H 44.0f
#define VOL_PANEL_Y 412.0f
#define VOL_LABEL_Y 387.0f
#define VOL_LABEL_H 16.0f
#define VOL_BTN_X 5.3f
#define VOL_BTN_Y 6.0f
#define VOL_BTN_W 24.0f
#define VOL_BTN_H 32.0f
#define VOL_BAR_X 37.3f
#define VOL_BAR_Y 14.0f
#define VOL_BAR_W 185.3f
#define VOL_BAR_H 16.0f
#define SCRIM_Y 248.9f
#define CHIP_RIGHT 628.0f
#define CHIP_Y 16.0f
#define CHIP_H 57.8f
#define CHIP_PAD_LEFT 7.3f
#define CHIP_PAD_TOP 8.9f
#define CHIP_GAP 4.7f
#define CHIP_PAD_RIGHT 9.3f
#define CHIP_TILE_H 40.0f
#define CHIP_TILE_MIN_W 30.0f
#define CHIP_TILE_PAD 12.0f
#define CHIP_NAME_Y 84.4f
#define CHIP_NAME_H 28.4f
#define CHIP_NAME_PAD 28.0f
#define CHIP_NAME_MIN_W 60.0f

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

osd_layout osd_layout_for(float offset_y, float pixel_x, float name_w, float number_w, float start_w, float countdown_w)
{
    osd_layout l;
    float px = MARGIN_X;
    float py = SCREEN_H - MARGIN_BOTTOM - PANEL_H + offset_y;
    float pw = SCREEN_W - 2.0f * MARGIN_X;
    l.panel = rect(px, py, pw, PANEL_H);
    l.icon = rect(px + ICON_X, py + ICON_Y, ICON_PX * pixel_x, ICON_PX);
    float tx = px + TEXT_X;
    l.name = rect(tx, py + NAME_Y, name_w, NAME_H);
    l.number = rect(tx + name_w + NUMBER_GAP, py + NUMBER_Y, number_w < NUMBER_MIN_W ? NUMBER_MIN_W : number_w, NUMBER_H);
    l.line = rect(tx, py + LINE_Y, px + DIVIDER_X - NEXT_TEXT_GAP - tx, LINE_H);
    l.start = rect(tx, py + TIMES_Y, start_w, TIMES_H);
    float bx = tx + start_w + BAR_GAP;
    l.bar = rect(bx, py + BAR_Y, px + BAR_RIGHT - bx, BAR_H);
    l.end = rect(px + END_RIGHT - start_w, py + TIMES_Y, start_w, TIMES_H);
    l.divider = rect(px + DIVIDER_X, py + DIVIDER_Y, 1.0f, DIVIDER_H);
    l.next_icon = rect(px + NEXT_ICON_X, py + NEXT_ICON_Y, NEXT_ICON_PX * pixel_x, NEXT_ICON_PX);
    float cw = countdown_w < COUNTDOWN_MIN_W ? COUNTDOWN_MIN_W : countdown_w;
    l.countdown = rect(px + COUNTDOWN_RIGHT - cw, py + COUNTDOWN_Y, cw, COUNTDOWN_H);
    float nx = l.next_icon.x + l.next_icon.w + NEXT_TEXT_GAP;
    float nw = px + NEXT_RIGHT - nx;
    float room = l.countdown.x - COUNTDOWN_GAP - nx;
    if (nw > room) nw = room;
    l.next_label = rect(nx, py + NEXT_LABEL_Y, nw, NEXT_LABEL_H);
    l.next_title = rect(nx, py + NEXT_TITLE_Y, nw, NEXT_TITLE_H);
    l.next_at = rect(nx, py + NEXT_AT_Y, nw, NEXT_AT_H);

    float vx = (SCREEN_W - VOL_PANEL_W) / 2.0f;
    l.vol_label = rect(vx, VOL_LABEL_Y, VOL_PANEL_W, VOL_LABEL_H);
    l.vol_panel = rect(vx, VOL_PANEL_Y, VOL_PANEL_W, VOL_PANEL_H);
    l.vol_down = rect(vx + VOL_BTN_X, VOL_PANEL_Y + VOL_BTN_Y, VOL_BTN_W, VOL_BTN_H);
    l.vol_up = rect(vx + VOL_PANEL_W - VOL_BTN_X - VOL_BTN_W, VOL_PANEL_Y + VOL_BTN_Y, VOL_BTN_W, VOL_BTN_H);
    l.vol_bar = rect(vx + VOL_BAR_X, VOL_PANEL_Y + VOL_BAR_Y, VOL_BAR_W, VOL_BAR_H);
    return l;
}

osd_chip osd_chip_layout(float number_w, float name_w, float pixel_x)
{
    osd_chip c;
    float nw = number_w < 0.0f ? 0.0f : number_w;
    if (nw > OSD_CHIP_NUMBER_MAX_W) nw = OSD_CHIP_NUMBER_MAX_W;
    float tile_w = nw + CHIP_TILE_PAD;
    if (tile_w < CHIP_TILE_MIN_W) tile_w = CHIP_TILE_MIN_W;
    float icon_w = CHIP_TILE_H * pixel_x;
    float w = CHIP_PAD_LEFT + tile_w + CHIP_GAP + icon_w + CHIP_PAD_RIGHT;
    float x = CHIP_RIGHT - w;
    c.chip = rect(x, CHIP_Y, w, CHIP_H);
    c.tile = rect(x + CHIP_PAD_LEFT, CHIP_Y + CHIP_PAD_TOP, tile_w, CHIP_TILE_H);
    c.icon = rect(c.tile.x + tile_w + CHIP_GAP, CHIP_Y + CHIP_PAD_TOP, icon_w, CHIP_TILE_H);
    float pw = (name_w < 0.0f ? 0.0f : name_w) + CHIP_NAME_PAD;
    if (pw < CHIP_NAME_MIN_W) pw = CHIP_NAME_MIN_W;
    c.name_pill = rect(CHIP_RIGHT - pw, CHIP_NAME_Y, pw, CHIP_NAME_H);
    return c;
}

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

osd_btn osd_hit(const osd_layout *l, float x, float y, bool volume_visible)
{
    if (!volume_visible) return OSD_BTN_NONE;
    if (inside(l->vol_down, x, y)) return OSD_BTN_VOL_DOWN;
    if (inside(l->vol_up, x, y)) return OSD_BTN_VOL_UP;
    return OSD_BTN_NONE;
}

asp_rect osd_scrim(void)
{
    return rect(0.0f, SCRIM_Y, SCREEN_W, SCREEN_H - SCRIM_Y);
}

asp_rect osd_segment(asp_rect bar, int index)
{
    float w = (bar.w - SEGMENT_GAP * (float)(OSD_VOLUME_SEGMENTS - 1)) / (float)OSD_VOLUME_SEGMENTS;
    return rect(bar.x + (w + SEGMENT_GAP) * (float)index, bar.y, w, bar.h);
}

int osd_segments_lit(int volume)
{
    if (volume <= 0) return 0;
    if (volume >= 100) return OSD_VOLUME_SEGMENTS;
    return (volume + 9) / 10;
}
