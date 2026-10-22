#ifndef CTW_MEDIA_YUVGX_H
#define CTW_MEDIA_YUVGX_H

#include <stdint.h>

#define YG_SLOTS 3

typedef struct {
    const uint8_t *data;
    int width, height, stride;
} yg_plane;

int yg_init(int w, int h);
void yg_free(void);
void yg_upload(int slot, const yg_plane *y, const yg_plane *cb, const yg_plane *cr);
void yg_draw(int slot, int frame_w, int frame_h, float x, float y, float w, float h);

#endif
