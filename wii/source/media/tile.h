#ifndef CTW_MEDIA_TILE_H
#define CTW_MEDIA_TILE_H

#include <stddef.h>
#include <stdint.h>

#define TILE_I8_W 8
#define TILE_I8_H 4

size_t tile_i8_index(int x, int y, int tex_w);
void tile_i8(uint8_t *dst, int tex_w, int tex_h, const uint8_t *src, int src_w, int src_h, int src_stride, uint8_t fill);

#endif
