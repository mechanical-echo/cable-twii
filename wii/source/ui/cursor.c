#include "cursor.h"

#include <grrlib.h>

#include "cursor_png.h"
#include "cursor_shadow_png.h"
#include "../input.h"
#include "draw.h"

static GRRLIB_texImg *tex_cursor;
static GRRLIB_texImg *tex_shadow;
static float ir_x, ir_y, ir_angle;
static bool ir_valid;
static bool blocked;

void cur_init(void)
{
    tex_cursor = GRRLIB_LoadTexture(cursor_png);
    tex_shadow = GRRLIB_LoadTexture(cursor_shadow_png);
    GRRLIB_SetMidHandle(tex_cursor, true);
    GRRLIB_SetMidHandle(tex_shadow, true);
}

void cur_update(void)
{
    ir_valid = in_ir(&ir_x, &ir_y, &ir_angle);
}

void cur_block(bool on)
{
    blocked = on;
}

bool cur_visible(void)
{
    return ir_valid && !blocked;
}

bool cur_raw_visible(void)
{
    return ir_valid;
}

float cur_x(void)
{
    return ir_x;
}

float cur_y(void)
{
    return ir_y;
}

void cur_draw(void)
{
    if (!ir_valid) return;
    float sx = ui_pixel_x();
    GRRLIB_DrawImg(ir_x + 3.0f * sx, ir_y + 3.0f, tex_shadow, ir_angle, sx, 1.0f, RGBA(0, 0, 0, 0xFF));
    GRRLIB_DrawImg(ir_x, ir_y, tex_cursor, ir_angle, sx, 1.0f, RGBA(0xFF, 0xFF, 0xFF, 0xFF));
}
