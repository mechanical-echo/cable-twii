#include <string.h>
#include "greatest/greatest.h"
#include "media/tile.h"

static uint8_t src[64 * 32];
static uint8_t dst[64 * 32];

static uint8_t pattern(int x, int y)
{
    return (uint8_t)(x * 7 + y * 13 + 1);
}

static void fill_src(int w, int h, int stride)
{
    memset(src, 0xEE, sizeof src);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) src[y * stride + x] = pattern(x, y);
    }
}

TEST index_first_tile(void)
{
    ASSERT_EQ((size_t)0, tile_i8_index(0, 0, 16));
    ASSERT_EQ((size_t)7, tile_i8_index(7, 0, 16));
    ASSERT_EQ((size_t)8, tile_i8_index(0, 1, 16));
    ASSERT_EQ((size_t)31, tile_i8_index(7, 3, 16));
    PASS();
}

TEST index_next_tile_in_row(void)
{
    ASSERT_EQ((size_t)32, tile_i8_index(8, 0, 16));
    ASSERT_EQ((size_t)32 + 8 + 2, tile_i8_index(10, 1, 16));
    PASS();
}

TEST index_next_tile_row(void)
{
    ASSERT_EQ((size_t)64, tile_i8_index(0, 4, 16));
    ASSERT_EQ((size_t)(3 * 32 * 3 + 32 + 8 * 2 + 1), tile_i8_index(9, 14, 24));
    PASS();
}

TEST tiles_with_wider_stride(void)
{
    fill_src(16, 8, 40);
    memset(dst, 0, sizeof dst);
    tile_i8(dst, 16, 8, src, 16, 8, 40, 0);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 16; x++) ASSERT_EQ(pattern(x, y), dst[tile_i8_index(x, y, 16)]);
    }
    PASS();
}

TEST pads_beyond_source(void)
{
    fill_src(12, 6, 12);
    memset(dst, 0, sizeof dst);
    tile_i8(dst, 16, 8, src, 12, 6, 12, 0x80);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 16; x++) {
            uint8_t want = (x < 12 && y < 6) ? pattern(x, y) : 0x80;
            ASSERT_EQ(want, dst[tile_i8_index(x, y, 16)]);
        }
    }
    PASS();
}

TEST chroma_plane_of_odd_macroblocks(void)
{
    fill_src(24, 12, 24);
    memset(dst, 0, sizeof dst);
    tile_i8(dst, 24, 12, src, 24, 12, 24, 0);
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x < 24; x++) ASSERT_EQ(pattern(x, y), dst[tile_i8_index(x, y, 24)]);
    }
    PASS();
}

SUITE(tile_suite)
{
    RUN_TEST(index_first_tile);
    RUN_TEST(index_next_tile_in_row);
    RUN_TEST(index_next_tile_row);
    RUN_TEST(tiles_with_wider_stride);
    RUN_TEST(pads_beyond_source);
    RUN_TEST(chroma_plane_of_odd_macroblocks);
}
