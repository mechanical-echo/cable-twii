#include "tile.h"

#include <string.h>

size_t tile_i8_index(int x, int y, int tex_w)
{
    size_t tiles_per_row = (size_t)(tex_w / TILE_I8_W);
    size_t tile = (size_t)(y / TILE_I8_H) * tiles_per_row + (size_t)(x / TILE_I8_W);
    return tile * (TILE_I8_W * TILE_I8_H) + (size_t)(y % TILE_I8_H) * TILE_I8_W + (size_t)(x % TILE_I8_W);
}

void tile_i8(uint8_t *dst, int tex_w, int tex_h, const uint8_t *src, int src_w, int src_h, int src_stride, uint8_t fill)
{
    uint8_t *d = dst;
    for (int ty = 0; ty < tex_h; ty += TILE_I8_H) {
        for (int tx = 0; tx < tex_w; tx += TILE_I8_W) {
            for (int r = 0; r < TILE_I8_H; r++) {
                int y = ty + r;
                int n = y < src_h ? src_w - tx : 0;
                if (n > TILE_I8_W) n = TILE_I8_W;
                if (n < 0) n = 0;
                if (n > 0) memcpy(d, src + (size_t)y * (size_t)src_stride + (size_t)tx, (size_t)n);
                if (n < TILE_I8_W) memset(d + n, fill, (size_t)(TILE_I8_W - n));
                d += TILE_I8_W;
            }
        }
    }
}
