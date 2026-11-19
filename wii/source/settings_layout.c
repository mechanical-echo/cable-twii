#include "settings_layout.h"

#define SETTINGS_X 84.0f
#define SETTINGS_RIGHT 556.0f
#define SOURCE_Y 115.0f
#define SOURCE_HALF_W 120.0f
#define SOURCE_H 40.0f
#define SOURCE_X 216.0f
#define FIELD_Y 169.0f
#define FIELD_W 254.0f
#define EDIT_X 480.0f
#define EDIT_W 76.0f
#define TEST_Y 223.0f
#define TEST_W 170.0f
#define DOT_W 8.0f
#define DOT_GAP 8.0f
#define RESULT_Y 273.0f
#define RESULT_LINES 2.0f
#define RESULT_LINE_H 18.6f
#define BUTTON_Y 369.0f
#define BUTTON_W 120.0f
#define CANCEL_X 306.0f
#define SAVE_X 436.0f
#define BUTTON_H 40.0f
#define TEXT_X 63.0f
#define TEXT_Y 110.0f
#define TEXT_W 514.0f
#define TEXT_H 44.0f
#define KEY_X 63.0f
#define KEY_Y 168.0f
#define KEY_W 46.0f
#define KEY_H 40.0f
#define KEY_STEP_X 52.0f
#define KEY_STEP_Y 46.0f
#define KEY_GROW 3.0f
#define BOTTOM_Y 352.0f
#define HTTP_W 150.0f
#define BACK_X 219.0f
#define DONE_X 375.0f
#define DONE_W 202.0f
#define KEY_COLS 10
#define CHAR_ROWS 4

static asp_rect rect(float x, float y, float w, float h)
{
    asp_rect r = { x, y, w, h };
    return r;
}

settings_layout setl_make(float pixel_x)
{
    settings_layout l = { 0 };
    l.drive = rect(SOURCE_X, SOURCE_Y, SOURCE_HALF_W, SOURCE_H);
    l.server = rect(SOURCE_X + SOURCE_HALF_W, SOURCE_Y, SOURCE_HALF_W, SOURCE_H);
    l.field = rect(SOURCE_X, FIELD_Y, FIELD_W, SOURCE_H);
    l.edit = rect(EDIT_X, FIELD_Y, EDIT_W, SOURCE_H);
    l.test = rect(SETTINGS_X, TEST_Y, TEST_W, SOURCE_H);
    float text_x = SETTINGS_X + DOT_W * pixel_x + DOT_GAP;
    l.result = rect(text_x, RESULT_Y, SETTINGS_RIGHT - text_x, RESULT_LINES * RESULT_LINE_H);
    l.cancel = rect(CANCEL_X, BUTTON_Y, BUTTON_W, BUTTON_H);
    l.save = rect(SAVE_X, BUTTON_Y, BUTTON_W, BUTTON_H);
    l.text = rect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H);
    for (int r = 0; r < CHAR_ROWS; r++) {
        for (int c = 0; c < KEY_COLS; c++) {
            l.keys[r][c] = rect(KEY_X + KEY_STEP_X * (float)c, KEY_Y + KEY_STEP_Y * (float)r, KEY_W, KEY_H);
        }
    }
    l.keys[CHAR_ROWS][0] = rect(KEY_X, BOTTOM_Y, HTTP_W, KEY_H);
    l.keys[CHAR_ROWS][1] = rect(BACK_X, BOTTOM_Y, HTTP_W, KEY_H);
    l.keys[CHAR_ROWS][2] = rect(DONE_X, BOTTOM_Y, DONE_W, KEY_H);
    return l;
}

static bool inside(asp_rect r, float x, float y)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

set_btn setl_hit(const settings_layout *l, float x, float y)
{
    if (inside(l->drive, x, y)) return SET_BTN_DRIVE;
    if (inside(l->server, x, y)) return SET_BTN_SERVER;
    if (inside(l->edit, x, y) || inside(l->field, x, y)) return SET_BTN_EDIT;
    if (inside(l->test, x, y)) return SET_BTN_TEST;
    if (inside(l->save, x, y)) return SET_BTN_SAVE;
    if (inside(l->cancel, x, y)) return SET_BTN_CANCEL;
    return SET_BTN_NONE;
}

bool setl_key_hit(const settings_layout *l, float x, float y, int *row, int *col)
{
    for (int r = 0; r < KB_ROWS; r++) {
        for (int c = 0; c < kb_row_len(r); c++) {
            asp_rect k = l->keys[r][c];
            asp_rect grown = rect(k.x - KEY_GROW, k.y - KEY_GROW, k.w + 2.0f * KEY_GROW, k.h + 2.0f * KEY_GROW);
            if (inside(grown, x, y)) {
                *row = r;
                *col = c;
                return true;
            }
        }
    }
    return false;
}
