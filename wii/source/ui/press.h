#ifndef CTW_UI_PRESS_H
#define CTW_UI_PRESS_H

#include <stdbool.h>

typedef struct {
    int armed;
} ps_state;

int ps_update(ps_state *s, int hot, bool pressed, bool held, bool released);
int ps_pressed(const ps_state *s, int hot);

#endif
