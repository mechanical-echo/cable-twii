#include "osd_layout.h"

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define MARGIN_X 26.0f
#define MARGIN_BOTTOM 24.0f
#define PANEL_H 108.0f
#define PAD 14.0f
#define TEXT_W 320.0f
#define ICON_PX 44.0f
#define PILL_W 70.0f
#define PILL_H 30.0f
#define PILL_GAP 6.0f
#define VOL_BTN_W 34.0f
#define VOL_GAP 8.0f
#define SEGMENT_GAP 3.0f
#define VOL_PANEL_W 260.0f
#define VOL_PANEL_H 44.0f
#define CHIP_MARGIN 16.0f
#define CHIP_H 56.0f
#define CHIP_PAD_LEFT 8.0f
#define CHIP_GAP 10.0f
#define CHIP_PAD_RIGHT 18.0f
#define CHIP_NUMBER_INK_MID 23.0f

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

osd_layout osd_layout_for(float offset_y)
{
    osd_layout l;
    float px = MARGIN_X;
    float py = SCREEN_H - MARGIN_BOTTOM - PANEL_H + offset_y;
    float pw = SCREEN_W - 2.0f * MARGIN_X;
    l.panel = rect(px, py, pw, PANEL_H);
    l.name = rect(px + PAD, py + 10.0f, TEXT_W, 20.0f);
    l.icon = rect(px + PAD, py + 34.0f, ICON_PX, ICON_PX);
    float tx = px + PAD + ICON_PX + 10.0f;
    l.line = rect(tx, py + 36.0f, TEXT_W - ICON_PX - 10.0f, 20.0f);
    l.times = rect(tx, py + 58.0f, TEXT_W - ICON_PX - 10.0f, 16.0f);
    l.bar = rect(px + PAD, py + 94.0f, TEXT_W, 5.0f);

    float rx = px + PAD + TEXT_W + PAD;
    l.prev = rect(rx, py + 14.0f, PILL_W, PILL_H);
    l.next = rect(rx + PILL_W + PILL_GAP, py + 14.0f, PILL_W, PILL_H);
    l.guide = rect(rx + 2.0f * (PILL_W + PILL_GAP), py + 14.0f, PILL_W, PILL_H);

    float right = px + pw - PAD;
    float vy = py + 64.0f;
    l.vol_down = rect(rx, vy, VOL_BTN_W, PILL_H);
    l.vol_up = rect(right - VOL_BTN_W, vy, VOL_BTN_W, PILL_H);
    float bx = rx + VOL_BTN_W + VOL_GAP;
    l.vol_bar = rect(bx, vy + 9.0f, l.vol_up.x - VOL_GAP - bx, 12.0f);

    float vx = (SCREEN_W - VOL_PANEL_W) / 2.0f;
    float vpy = SCREEN_H - MARGIN_BOTTOM - VOL_PANEL_H;
    l.vol_panel = rect(vx, vpy, VOL_PANEL_W, VOL_PANEL_H);
    l.vol_panel_bar = rect(vx + 100.0f, vpy + 16.0f, 144.0f, 12.0f);
    return l;
}

osd_chip osd_chip_layout(float number_w)
{
    osd_chip c;
    float nw = number_w < 0.0f ? 0.0f : number_w;
    if (nw > OSD_CHIP_NUMBER_MAX_W) nw = OSD_CHIP_NUMBER_MAX_W;
    float w = CHIP_PAD_LEFT + OSD_CHIP_ICON_PX + CHIP_GAP + nw + CHIP_PAD_RIGHT;
    float x = SCREEN_W - CHIP_MARGIN - w;
    float y = CHIP_MARGIN;
    c.chip = rect(x, y, w, CHIP_H);
    c.icon = rect(x + CHIP_PAD_LEFT, y + (CHIP_H - OSD_CHIP_ICON_PX) / 2.0f, OSD_CHIP_ICON_PX, OSD_CHIP_ICON_PX);
    c.number = rect(c.icon.x + OSD_CHIP_ICON_PX + CHIP_GAP, y + CHIP_H / 2.0f - CHIP_NUMBER_INK_MID, nw, (float)OSD_CHIP_NUMBER_PX);
    return c;
}

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

osd_btn osd_hit(const osd_layout *l, float x, float y)
{
    if (inside(l->prev, x, y)) return OSD_BTN_PREV;
    if (inside(l->next, x, y)) return OSD_BTN_NEXT;
    if (inside(l->guide, x, y)) return OSD_BTN_GUIDE;
    if (inside(l->vol_down, x, y)) return OSD_BTN_VOL_DOWN;
    if (inside(l->vol_up, x, y)) return OSD_BTN_VOL_UP;
    return OSD_BTN_NONE;
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
