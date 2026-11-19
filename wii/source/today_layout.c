#include "today_layout.h"

#define CARD_X 30.0f
#define CARD_Y 22.0f
#define CARD_W 580.0f
#define CARD_H 436.0f
#define ICON_X 14.0f
#define ICON_Y 12.0f
#define ICON_PX 36.0f
#define NAME_GAP 9.0f
#define NAME_PX 18.0f
#define TAG_ROOM 80.0f
#define DATE_Y 35.0f
#define DATE_PX 12.0f
#define TAG_RIGHT 16.0f
#define TAG_Y 17.0f
#define TAG_H 20.0f
#define RULE_X 12.0f
#define RULE_TOP_Y 58.0f
#define LIST_X 8.0f
#define LIST_Y 64.0f
#define LIST_RIGHT 22.0f
#define LIST_BOTTOM 44.0f
#define SCROLL_RIGHT 12.3f
#define SCROLL_W 3.0f
#define SCROLL_INSET 4.0f
#define RULE_BOTTOM 40.0f
#define HINTS_X 18.0f
#define HINTS_BOTTOM 30.0f
#define NOTE_RIGHT 18.0f
#define NOTE_DY 4.0f
#define NOTE_PX 12.0f
#define COL_TIME 14.0f
#define COL_GAP 12.0f
#define COL_ICON 22.0f
#define COL_SHOW_GAP 4.0f
#define COL_SHOW_W 110.0f
#define COL_CODE_GAP 8.0f
#define COL_TITLE_GAP 10.0f
#define COL_RIGHT 10.0f
#define HINT_DISC_X 2.7f
#define HINT_LABEL_GAP 4.0f
#define HINT_PAD_RIGHT 9.0f
#define HINT_ABOVE 8.0f
#define FRAME_PX 5.0f
#define WATCH_X 9.0f
#define WATCH_BOTTOM 33.8f
#define WATCH_W 73.3f
#define WATCH_GAP 6.0f

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

today_layout today_layout_for(float pixel_x)
{
    today_layout l;
    l.card = rect(CARD_X, CARD_Y, CARD_W, CARD_H);
    l.icon = rect(CARD_X + ICON_X, CARD_Y + ICON_Y, ICON_PX * pixel_x, ICON_PX);
    float tx = l.icon.x + l.icon.w + NAME_GAP;
    float tag_right = CARD_X + CARD_W - TAG_RIGHT;
    l.name = rect(tx, CARD_Y + ICON_Y, tag_right - TAG_ROOM - tx, NAME_PX);
    l.date = rect(tx, CARD_Y + DATE_Y, l.name.w, DATE_PX);
    l.tag = rect(tag_right, CARD_Y + TAG_Y, 0.0f, TAG_H);
    l.rule_top = rect(CARD_X + RULE_X, CARD_Y + RULE_TOP_Y, CARD_W - 2.0f * RULE_X, 1.0f);
    l.list = rect(CARD_X + LIST_X, CARD_Y + LIST_Y, CARD_W - LIST_RIGHT - LIST_X, CARD_H - LIST_Y - LIST_BOTTOM);
    l.scroll = rect(CARD_X + CARD_W - SCROLL_RIGHT, l.list.y + SCROLL_INSET, SCROLL_W, l.list.h - 2.0f * SCROLL_INSET);
    l.rule_bottom = rect(CARD_X + RULE_X, CARD_Y + CARD_H - RULE_BOTTOM, CARD_W - 2.0f * RULE_X, 1.0f);
    l.hints = rect(CARD_X + HINTS_X, CARD_Y + CARD_H - HINTS_BOTTOM, 0.0f, TL_HINT_DISC);
    l.skip_note = rect(CARD_X + CARD_W - NOTE_RIGHT, l.hints.y + NOTE_DY, 0.0f, NOTE_PX);
    return l;
}

today_cols today_cols_for(asp_rect row, float time_w, float code_w, float pixel_x)
{
    today_cols c;
    c.time_x = row.x + COL_TIME * pixel_x;
    c.icon_x = c.time_x + time_w + COL_GAP * pixel_x;
    c.icon_w = COL_ICON * pixel_x;
    c.show_x = c.icon_x + (COL_ICON + COL_SHOW_GAP) * pixel_x;
    c.show_w = COL_SHOW_W * pixel_x;
    c.code_x = c.show_x + c.show_w + COL_CODE_GAP * pixel_x;
    c.title_x = c.code_x + code_w + COL_TITLE_GAP * pixel_x;
    c.title_w = row.x + row.w - COL_RIGHT * pixel_x - c.title_x;
    return c;
}

float today_hint_w(float label_w, float pixel_x)
{
    return HINT_DISC_X + TL_HINT_DISC * pixel_x + HINT_LABEL_GAP + label_w + HINT_PAD_RIGHT * pixel_x;
}

asp_rect today_hint_full(asp_rect panel, float hint_w)
{
    return rect(panel.x + panel.w - hint_w, panel.y - HINT_ABOVE - TL_HINT_H, hint_w, TL_HINT_H);
}

asp_rect today_hint_guide(asp_rect preview, float hint_w, float pixel_x)
{
    float in_x = preview.x + FRAME_PX * pixel_x;
    float in_y = preview.y + FRAME_PX;
    float in_h = preview.h - 2.0f * FRAME_PX;
    return rect(in_x + WATCH_X + WATCH_W + WATCH_GAP, in_y + in_h - WATCH_BOTTOM, hint_w, TL_HINT_H);
}
