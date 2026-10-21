#ifndef CTW_MEDIA_YUVGX_H
#define CTW_MEDIA_YUVGX_H

#include <stdint.h>

#define YG_SLOTS 3

int yg_init(int w, int h);
void yg_free(void);
void yg_upload(int slot, const uint8_t *y, const uint8_t *cb, const uint8_t *cr, int w, int h);
void yg_draw(int slot, int frame_w, int frame_h, float x, float y, float w, float h);

#endif
