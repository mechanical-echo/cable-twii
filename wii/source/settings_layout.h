#ifndef CTW_SETTINGS_LAYOUT_H
#define CTW_SETTINGS_LAYOUT_H

#include <stdbool.h>
#include "aspect.h"
#include "keyboard.h"

typedef enum {
    SET_BTN_NONE,
    SET_BTN_DRIVE,
    SET_BTN_SERVER,
    SET_BTN_EDIT,
    SET_BTN_TEST,
    SET_BTN_SAVE,
    SET_BTN_CANCEL
} set_btn;

typedef struct {
    asp_rect card, title, source_label, track, address_label, field_text;
    asp_rect drive, server, field, edit, test, result, save, cancel;
    asp_rect drive_thumb, server_thumb, result_dot, divider, no_sd_dot, no_sd;
    asp_rect kb_card, kb_label, typed, caret;
    asp_rect keys[KB_ROWS][10];
    asp_rect text;
} settings_layout;

settings_layout setl_make(float pixel_x);
set_btn setl_hit(const settings_layout *l, float x, float y);
bool setl_key_hit(const settings_layout *l, float x, float y, int *row, int *col);

#endif
