#include "settings_layout.h"

#define CARD_X 60.0f
#define CARD_Y 47.0f
#define CARD_W 520.0f
#define CARD_H 386.0f
#define SETTINGS_X 84.0f
#define SETTINGS_RIGHT 556.0f
#define TITLE_Y 71.0f
#define TITLE_H 20.0f
#define LABEL_W 120.0f
#define LABEL_H 15.0f
#define LABEL_DY 12.5f
#define FIELD_PAD 12.0f
#define FIELD_TEXT_H 16.0f
#define FIELD_TEXT_W 230.0f
#define THUMB_INSET 4.0f
#define DOT_DY 5.0f
#define DIVIDER_Y 325.0f
#define NO_SD_Y 339.0f
#define NO_SD_H 13.0f
#define KB_CARD_X 43.0f
#define KB_CARD_Y 68.0f
#define KB_CARD_W 554.0f
#define KB_CARD_H 344.0f
#define KB_LABEL_Y 86.0f
#define TYPED_PAD 14.0f
#define TYPED_DY 12.0f
#define TYPED_H 20.0f
#define CARET_DY 10.0f
#define CARET_W 2.0f
#define CARET_H 24.0f
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

static asp_rect thumb(asp_rect half, float pixel_x)
{
    return rect(half.x + THUMB_INSET * pixel_x, half.y + THUMB_INSET, half.w - 2.0f * THUMB_INSET * pixel_x, half.h - 2.0f * THUMB_INSET);
}

settings_layout setl_make(float pixel_x)
{
    settings_layout l = { 0 };
    l.card = rect(CARD_X, CARD_Y, CARD_W, CARD_H);
    l.title = rect(SETTINGS_X, TITLE_Y, SETTINGS_RIGHT - SETTINGS_X, TITLE_H);
    l.source_label = rect(SETTINGS_X, SOURCE_Y + LABEL_DY, LABEL_W, LABEL_H);
    l.track = rect(SOURCE_X, SOURCE_Y, 2.0f * SOURCE_HALF_W, SOURCE_H);
    l.drive = rect(SOURCE_X, SOURCE_Y, SOURCE_HALF_W, SOURCE_H);
    l.server = rect(SOURCE_X + SOURCE_HALF_W, SOURCE_Y, SOURCE_HALF_W, SOURCE_H);
    l.drive_thumb = thumb(l.drive, pixel_x);
    l.server_thumb = thumb(l.server, pixel_x);
    l.address_label = rect(SETTINGS_X, FIELD_Y + LABEL_DY, LABEL_W, LABEL_H);
    l.field = rect(SOURCE_X, FIELD_Y, FIELD_W, SOURCE_H);
    l.field_text = rect(SOURCE_X + FIELD_PAD, FIELD_Y + (SOURCE_H - FIELD_TEXT_H) / 2.0f, FIELD_TEXT_W, FIELD_TEXT_H);
    l.edit = rect(EDIT_X, FIELD_Y, EDIT_W, SOURCE_H);
    l.test = rect(SETTINGS_X, TEST_Y, TEST_W, SOURCE_H);
    float text_x = SETTINGS_X + DOT_W * pixel_x + DOT_GAP;
    l.result_dot = rect(SETTINGS_X, RESULT_Y + DOT_DY, DOT_W * pixel_x, DOT_W);
    l.result = rect(text_x, RESULT_Y, SETTINGS_RIGHT - text_x, RESULT_LINES * RESULT_LINE_H);
    l.divider = rect(SETTINGS_X, DIVIDER_Y, SETTINGS_RIGHT - SETTINGS_X, 1.0f);
    l.no_sd_dot = rect(SETTINGS_X, NO_SD_Y + DOT_DY, DOT_W * pixel_x, DOT_W);
    l.no_sd = rect(text_x, NO_SD_Y, SETTINGS_RIGHT - text_x, NO_SD_H);
    l.cancel = rect(CANCEL_X, BUTTON_Y, BUTTON_W, BUTTON_H);
    l.save = rect(SAVE_X, BUTTON_Y, BUTTON_W, BUTTON_H);
    l.kb_card = rect(KB_CARD_X, KB_CARD_Y, KB_CARD_W, KB_CARD_H);
    l.kb_label = rect(TEXT_X, KB_LABEL_Y, TEXT_W, LABEL_H);
    l.text = rect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H);
    l.typed = rect(TEXT_X + TYPED_PAD, TEXT_Y + TYPED_DY, TEXT_W - 2.0f * TYPED_PAD, TYPED_H);
    l.caret = rect(l.typed.x, TEXT_Y + CARET_DY, CARET_W * pixel_x, CARET_H);
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
