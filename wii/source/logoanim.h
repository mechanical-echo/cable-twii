#ifndef CTW_LOGOANIM_H
#define CTW_LOGOANIM_H

#include <stdbool.h>
#include <stdint.h>
#include "ui/theme.h"

#define LOGO_INTRO_END_MS 600u
#define LOGO_SHINE_END_MS 1400u
#define LOGO_HANDOFF_MS 2000u
#define LOGO_TOTAL_MS THEME_LOGO_DURATION_MS

typedef struct {
    float x;
    float y;
    float w;
    float h;
    float scale;
    float alpha;
    float shine;
    bool done;
} logo_frame;

void logo_header_rect_for(float img_w, float img_h, float *x, float *y, float *w, float *h);
logo_frame logo_frame_at(uint32_t t_ms, bool skipped, float img_w, float img_h);

#endif
