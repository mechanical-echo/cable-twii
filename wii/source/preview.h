#ifndef CTW_PREVIEW_H
#define CTW_PREVIEW_H

#include <stdint.h>
#include "ui/theme.h"

#define PV_DEBOUNCE_MS THEME_HOVER_DEBOUNCE_MS

typedef struct {
    int pending;
    uint32_t since_ms;
    int tuned;
} pv_state;

void pv_reset(pv_state *s, int tuned);
int pv_update(pv_state *s, int highlighted, uint32_t now_ms);
void pv_force(pv_state *s, int channel);
int pv_tuned(const pv_state *s);

#endif
