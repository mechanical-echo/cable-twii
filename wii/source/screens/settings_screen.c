#include "settings_screen.h"

#include <stdio.h>
#include <string.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "../input.h"
#include "../keyboard.h"
#include "../settings_layout.h"
#include "../srcpick.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/press.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"
#include "notice.h"

#define TITLE_PX 20
#define LABEL_PX 15
#define FIELD_PX 16
#define PILL_PX 14
#define LINE_PX 13
#define LINE_H 18.6f
#define LINE_MAX 2
#define LINE_BYTES 320
#define NAME_BYTES 128
#define TYPED_PX 20
#define KEY_PX 18
#define BOTTOM_KEY_PX 15
#define KEY_RADIUS 10.0f
#define KEY_COLS 10
#define CARD_MID 0.6f
#define KB_CARD_MID 0.55f
#define KB_DIM 0x00000099u
#define BORDER 2.0f
#define DOT_RING 2.0f
#define PRESS_DY 2.0f
#define CARET_GAP 2.0f
#define PILL_SHADOW_DY 1.0f
#define PILL_SHADOW_BLUR 4.0f

static const char NO_SD[] = "No SD card. These settings last until cable-twii closes.";

static settings_layout layout;
static ctw_settings edited;
static set_btn hot_btn;
static ps_state press;
static bool typing;
static kb_state kb;
static int hot_key;
static int last_key;
static bool testing;
static sp_line line;
static char line_name[NAME_BYTES];
static tf_line line_text[LINE_MAX];
static size_t line_count;
static char field_text[TF_LINE_BYTES];
static char typed_text[TF_LINE_BYTES];
static float typed_w;
static char no_sd_text[TF_LINE_BYTES];

static int key_id(int row, int col)
{
    return row * KEY_COLS + col + 1;
}

static void refresh_field(void)
{
    ui_text_fit(FONT_BOLD, FIELD_PX, edited.server, layout.field_text.w, field_text, sizeof field_text);
}

static void refresh_typed(void)
{
    ui_text_fit_tail(FONT_BOLD, TYPED_PX, kb.text, layout.typed.w, typed_text, sizeof typed_text);
    typed_w = ui_text_width(FONT_BOLD, TYPED_PX, typed_text);
}

static void show_line(sp_line kind, const char *text)
{
    line = kind;
    line_count = kind == SP_LINE_NONE ? 0 : ui_text_wrap(FONT_BODY, LINE_PX, text, layout.result.w, LINE_MAX, line_text);
}

void settings_enter(app *a)
{
    layout = setl_make(ui_pixel_x());
    edited = a->settings;
    hot_btn = SET_BTN_NONE;
    press.armed = 0;
    typing = false;
    testing = false;
    show_line(SP_LINE_NONE, "");
    refresh_field();
    ui_text_fit(FONT_BODY, LINE_PX, NO_SD, layout.no_sd.w, no_sd_text, sizeof no_sd_text);
}

static void leave(app *a)
{
    typing = false;
    testing = false;
    app_go(a, a->prev_screen);
}

static void start_test(void)
{
    char text[LINE_BYTES];
    srv_addr addr;
    sp_line kind = sp_test_start(edited.server, &addr, line_name, sizeof line_name, text, sizeof text);
    testing = kind == SP_LINE_BUSY;
    if (testing) app_test_server(&addr);
    show_line(kind, text);
}

void settings_test_done(app *a, srv_result r, const srv_info *info)
{
    if (a->screen != SCR_SETTINGS || !testing) return;
    testing = false;
    char text[LINE_BYTES];
    show_line(sp_test_result(r, line_name, info->name, text, sizeof text), text);
}

static void open_keyboard(void)
{
    kb_init(&kb, edited.server);
    typing = true;
    hot_btn = SET_BTN_NONE;
    press.armed = 0;
    hot_key = key_id(kb.row, kb.col);
    last_key = 0;
    refresh_typed();
}

static void save(app *a)
{
    sp_save plan = sp_save_plan(&a->settings, &edited);
    if (plan == SP_SAVE_TYPE) {
        open_keyboard();
        return;
    }
    if (plan == SP_SAVE_BAD) {
        start_test();
        return;
    }
    if (plan == SP_SAVE_RETURN) {
        leave(a);
        return;
    }
    if (a->settings_saveable) settings_save(SETTINGS_DIR, &edited);
    if (plan == SP_SAVE_SWITCH) {
        typing = false;
        testing = false;
        app_switch_source(a, &edited);
        return;
    }
    a->settings = edited;
    leave(a);
}

static void close_keyboard(void)
{
    typing = false;
    press.armed = 0;
}

static void finish_typing(void)
{
    if (strcmp(kb.text, edited.server) != 0) {
        snprintf(edited.server, sizeof edited.server, "%s", kb.text);
        testing = false;
        show_line(SP_LINE_NONE, "");
        refresh_field();
    }
    close_keyboard();
}

static void type_key(int id)
{
    sfx_play(SFX_SELECT);
    if (kb_press(&kb, kb_key_at((id - 1) / KEY_COLS, (id - 1) % KEY_COLS))) {
        finish_typing();
        return;
    }
    refresh_typed();
}

static void move_key(int dx, int dy)
{
    int row = kb.row, col = kb.col;
    kb_move(&kb, dx, dy);
    if (kb.row != row || kb.col != col) sfx_play(SFX_HOVER);
}

static void keyboard_update(void)
{
    if (in_pressed(WPAD_BUTTON_B)) {
        sfx_play(SFX_BACK);
        close_keyboard();
        return;
    }
    if (in_pressed(WPAD_BUTTON_LEFT)) move_key(-1, 0);
    else if (in_pressed(WPAD_BUTTON_RIGHT)) move_key(1, 0);
    else if (in_pressed(WPAD_BUTTON_UP)) move_key(0, -1);
    else if (in_pressed(WPAD_BUTTON_DOWN)) move_key(0, 1);

    int row = 0, col = 0, hit = 0;
    if (cur_visible() && setl_key_hit(&layout, cur_x(), cur_y(), &row, &col)) hit = key_id(row, col);
    if (hit != 0 && hit != last_key && hit != key_id(kb.row, kb.col)) {
        kb.row = row;
        kb.col = col;
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    last_key = hit;
    hot_key = key_id(kb.row, kb.col);
    int fired = ps_update(&press, hot_key, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired != 0) type_key(fired);
}

static void choose(app *a, set_btn btn)
{
    sfx_play(SFX_SELECT);
    switch (btn) {
    case SET_BTN_DRIVE:
        edited.source = SRC_DRIVE;
        break;
    case SET_BTN_SERVER:
        edited.source = SRC_SERVER;
        break;
    case SET_BTN_EDIT:
        open_keyboard();
        break;
    case SET_BTN_TEST:
        start_test();
        break;
    case SET_BTN_SAVE:
        save(a);
        break;
    case SET_BTN_CANCEL:
        leave(a);
        break;
    case SET_BTN_NONE:
        break;
    }
}

void settings_update(app *a)
{
    if (typing) {
        keyboard_update();
        return;
    }
    if (in_pressed(WPAD_BUTTON_B)) {
        sfx_play(SFX_BACK);
        leave(a);
        return;
    }
    set_btn btn = cur_visible() ? setl_hit(&layout, cur_x(), cur_y()) : SET_BTN_NONE;
    if (btn != SET_BTN_NONE && btn != hot_btn) {
        sfx_play(SFX_HOVER);
        in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
    }
    hot_btn = btn;
    int fired = ps_update(&press, (int)btn, in_pressed(WPAD_BUTTON_A), in_held(WPAD_BUTTON_A), in_released(WPAD_BUTTON_A));
    if (fired != 0) choose(a, (set_btn)fired);
}

static ui_pill_state state_for(set_btn btn)
{
    if (ps_pressed(&press, (int)hot_btn) == (int)btn && btn != SET_BTN_NONE) return UI_PILL_PRESSED;
    return hot_btn == btn ? UI_PILL_HOT : UI_PILL_REST;
}

static void field(asp_rect r)
{
    float px = ui_pixel_x();
    float radius = (float)THEME_ROW_RADIUS;
    float ix = r.x + BORDER * px, iy = r.y + BORDER, iw = r.w - 2.0f * BORDER * px, ih = r.h - 2.0f * BORDER;
    ui_rounded(r.x, r.y, r.w, r.h, radius, THEME_PANEL_BORDER);
    ui_rounded(ix, iy, iw, ih, radius - BORDER, THEME_TRACK_INSET);
    ui_rounded(ix, iy + 1.0f, iw, ih - 1.0f, radius - BORDER, THEME_PANEL_FILL);
}

static void dot(asp_rect r, uint32_t color, bool ring)
{
    if (ring) ui_ring(r.x, r.y, r.w, r.h, r.h / 2.0f, THEME_RING, DOT_RING);
    ui_rounded(r.x, r.y, r.w, r.h, r.h / 2.0f, color);
}

static void source_half(asp_rect half, asp_rect thumb, const char *label, bool chosen, set_btn btn)
{
    ui_pill_state state = state_for(btn);
    float dy = state == UI_PILL_PRESSED ? PRESS_DY : 0.0f;
    float cx = half.x + half.w / 2.0f;
    float ly = half.y + half.h / 2.0f - (float)LABEL_PX / 2.0f + dy;
    if (!chosen) {
        if (state != UI_PILL_REST) ui_pill_ex(thumb.x, thumb.y, thumb.w, thumb.h, NULL, LABEL_PX, state, UI_SHADOW_PANEL);
        ui_text(FONT_BOLD, LABEL_PX, cx, ly, label, state != UI_PILL_REST ? THEME_TEXT_PRIMARY : THEME_TEXT_LABEL, ALIGN_CENTER);
        return;
    }
    float px = ui_pixel_x();
    float y = thumb.y + dy, r = thumb.h / 2.0f;
    if (state != UI_PILL_PRESSED) ui_shadow(thumb.x, y, thumb.w, thumb.h, r, THEME_PILL_SHADOW, PILL_SHADOW_DY, PILL_SHADOW_BLUR);
    if (state == UI_PILL_HOT) ui_ring(thumb.x, y, thumb.w, thumb.h, r, THEME_RING_STRONG, THEME_RING_SPREAD);
    if (state == UI_PILL_PRESSED) ui_ring(thumb.x, y, thumb.w, thumb.h, r, THEME_RING, THEME_RING_SPREAD);
    ui_rounded(thumb.x, y, thumb.w, thumb.h, r, THEME_ACCENT_DARK);
    ui_accent_tile(thumb.x + BORDER * px, y + BORDER, thumb.w - 2.0f * BORDER * px, thumb.h - 2.0f * BORDER, r - BORDER);
    ui_text(FONT_BOLD, LABEL_PX, cx, ly, label, THEME_PANEL_FILL, ALIGN_CENTER);
}

static void result_line(void)
{
    if (line == SP_LINE_NONE) return;
    uint32_t color = line == SP_LINE_OK ? THEME_ACCENT : (line == SP_LINE_BUSY ? THEME_DOT_3 : THEME_WARM);
    dot(layout.result_dot, color, line == SP_LINE_OK);
    for (size_t i = 0; i < line_count; i++) {
        ui_text(FONT_BODY, LINE_PX, layout.result.x, layout.result.y + LINE_H * (float)i, line_text[i].text, THEME_TEXT_LABEL, ALIGN_LEFT);
    }
}

static void draw_settings(const app *a)
{
    const settings_layout *l = &layout;
    ui_stripes();
    ui_card(l->card.x, l->card.y, l->card.w, l->card.h, THEME_PANEL_RADIUS, CARD_MID, THEME_GLOSS_BOTTOM, UI_SHADOW_PANEL);
    ui_text(FONT_BOLD, TITLE_PX, l->title.x, l->title.y, "Settings", THEME_TEXT_PRIMARY, ALIGN_LEFT);

    ui_text(FONT_BOLD, LABEL_PX, l->source_label.x, l->source_label.y, "Play from", THEME_TEXT_LABEL, ALIGN_LEFT);
    asp_rect t = l->track;
    ui_rounded(t.x, t.y, t.w, t.h, t.h / 2.0f, THEME_TRACK_INSET);
    ui_rounded(t.x, t.y + 1.0f, t.w, t.h - 1.0f, t.h / 2.0f, THEME_BAR_TRACK);
    source_half(l->drive, l->drive_thumb, "Drive", edited.source == SRC_DRIVE, SET_BTN_DRIVE);
    source_half(l->server, l->server_thumb, "Server", edited.source == SRC_SERVER, SET_BTN_SERVER);

    ui_text(FONT_BOLD, LABEL_PX, l->address_label.x, l->address_label.y, "Server address", THEME_TEXT_LABEL, ALIGN_LEFT);
    field(l->field);
    if (edited.server[0] != '\0') ui_text(FONT_BOLD, FIELD_PX, l->field_text.x, l->field_text.y, field_text, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    else ui_text(FONT_BOLD, FIELD_PX, l->field_text.x, l->field_text.y, "Not set", THEME_TEXT_FADED, ALIGN_LEFT);
    ui_pill_ex(l->edit.x, l->edit.y, l->edit.w, l->edit.h, "Edit", PILL_PX, state_for(SET_BTN_EDIT), UI_SHADOW_PANEL);
    ui_pill_ex(l->test.x, l->test.y, l->test.w, l->test.h, "Test connection", PILL_PX, state_for(SET_BTN_TEST), UI_SHADOW_PANEL);
    result_line();

    GRRLIB_Rectangle(l->divider.x, l->divider.y, l->divider.w, l->divider.h, THEME_BAR_TRACK, 1);
    if (!a->settings_saveable) {
        dot(l->no_sd_dot, THEME_WARM, false);
        ui_text(FONT_BODY, LINE_PX, l->no_sd.x, l->no_sd.y, no_sd_text, THEME_TEXT_LABEL, ALIGN_LEFT);
    }
    ui_pill_ex(l->cancel.x, l->cancel.y, l->cancel.w, l->cancel.h, "Cancel", PILL_PX, state_for(SET_BTN_CANCEL), UI_SHADOW_PANEL);
    ui_pill_ex(l->save.x, l->save.y, l->save.w, l->save.h, "Save", PILL_PX, state_for(SET_BTN_SAVE), UI_SHADOW_PANEL);
}

static void draw_key(int row, int col)
{
    asp_rect k = layout.keys[row][col];
    int id = key_id(row, col);
    ui_pill_state state = ps_pressed(&press, hot_key) == id ? UI_PILL_PRESSED : (hot_key == id ? UI_PILL_HOT : UI_PILL_REST);
    kb_key key = kb_key_at(row, col);
    char ch[2] = { key.ch, '\0' };
    const char *label = ch;
    int px = BOTTOM_KEY_PX;
    uint32_t color = THEME_TEXT_PRIMARY;
    switch (key.kind) {
    case KB_KEY_CHAR:
        px = KEY_PX;
        break;
    case KB_KEY_HTTP:
        label = "http://";
        if (kb.len > 0) color = THEME_TEXT_FADED;
        break;
    case KB_KEY_BACK:
        label = "\xE2\x86\x90 Backspace";
        break;
    case KB_KEY_DONE:
        label = "Done";
        break;
    }
    ui_pill_radius(k.x, k.y, k.w, k.h, KEY_RADIUS, label, px, color, state, UI_SHADOW_PANEL);
}

static void draw_keyboard(void)
{
    const settings_layout *l = &layout;
    GRRLIB_Rectangle(0.0f, 0.0f, 640.0f, 480.0f, KB_DIM, 1);
    ui_card(l->kb_card.x, l->kb_card.y, l->kb_card.w, l->kb_card.h, THEME_PANEL_RADIUS, KB_CARD_MID, THEME_GLOSS_SOFT, UI_SHADOW_VIDEO);
    ui_text(FONT_BOLD, LABEL_PX, l->kb_label.x, l->kb_label.y, "Server address", THEME_TEXT_LABEL, ALIGN_LEFT);
    field(l->text);
    ui_text(FONT_BOLD, TYPED_PX, l->typed.x, l->typed.y, typed_text, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    float caret_x = l->typed.x + typed_w + CARET_GAP;
    if (caret_x > l->typed.x + l->typed.w) caret_x = l->typed.x + l->typed.w;
    GRRLIB_Rectangle(caret_x, l->caret.y, l->caret.w, l->caret.h, THEME_ACCENT, 1);
    for (int r = 0; r < KB_ROWS; r++) {
        for (int c = 0; c < kb_row_len(r); c++) draw_key(r, c);
    }
}

void settings_draw(app *a)
{
    draw_settings(a);
    if (typing) draw_keyboard();
    notice_draw(a);
}
