#include "keyboard.h"
#include <string.h>

#define CHAR_ROWS 4
#define CHAR_COLS 10
#define BOTTOM_KEYS 3

static const char *const ROWS[CHAR_ROWS] = { "1234567890", "qwertyuiop", "asdfghjkl.", "zxcvbnm:/-" };

void kb_init(kb_state *k, const char *text)
{
    memset(k, 0, sizeof *k);
    size_t n = strlen(text);
    if (n > KB_MAX_TEXT) n = KB_MAX_TEXT;
    memcpy(k->text, text, n);
    k->len = n;
}

int kb_row_len(int row)
{
    return row == CHAR_ROWS ? BOTTOM_KEYS : CHAR_COLS;
}

kb_key kb_key_at(int row, int col)
{
    kb_key key = { KB_KEY_CHAR, 0 };
    if (row == CHAR_ROWS) {
        key.kind = col == 0 ? KB_KEY_HTTP : col == 1 ? KB_KEY_BACK : KB_KEY_DONE;
    } else if (row >= 0 && row < CHAR_ROWS && col >= 0 && col < CHAR_COLS) {
        key.ch = ROWS[row][col];
    }
    return key;
}

void kb_move(kb_state *k, int dx, int dy)
{
    int row = k->row + dy;
    if (row < 0) row = 0;
    if (row > KB_ROWS - 1) row = KB_ROWS - 1;
    int col = k->col + dx;
    if (col < 0) col = 0;
    if (col > kb_row_len(row) - 1) col = kb_row_len(row) - 1;
    k->row = row;
    k->col = col;
}

static void append(kb_state *k, const char *s)
{
    size_t n = strlen(s);
    if (k->len + n > KB_MAX_TEXT) return;
    memcpy(k->text + k->len, s, n + 1);
    k->len += n;
}

bool kb_press(kb_state *k, kb_key key)
{
    switch (key.kind) {
    case KB_KEY_CHAR: {
        char s[2] = { key.ch, 0 };
        if (key.ch != 0) append(k, s);
        return false;
    }
    case KB_KEY_HTTP:
        if (k->len == 0) append(k, "http://");
        return false;
    case KB_KEY_BACK:
        if (k->len > 0) k->text[--k->len] = 0;
        return false;
    case KB_KEY_DONE:
        return true;
    }
    return false;
}
