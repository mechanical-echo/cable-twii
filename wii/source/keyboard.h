#ifndef CTW_KEYBOARD_H
#define CTW_KEYBOARD_H

#include <stdbool.h>
#include <stddef.h>

#define KB_ROWS 5
#define KB_MAX_TEXT 127

typedef enum { KB_KEY_CHAR, KB_KEY_HTTP, KB_KEY_BACK, KB_KEY_DONE } kb_kind;

typedef struct {
    kb_kind kind;
    char ch;
} kb_key;

typedef struct {
    char text[KB_MAX_TEXT + 1];
    size_t len;
    int row;
    int col;
} kb_state;

void kb_init(kb_state *k, const char *text);
int kb_row_len(int row);
kb_key kb_key_at(int row, int col);
void kb_move(kb_state *k, int dx, int dy);
bool kb_press(kb_state *k, kb_key key);

#endif
