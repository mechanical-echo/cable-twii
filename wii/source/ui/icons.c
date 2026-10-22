#include "icons.h"

#include <grrlib.h>

#include "draw.h"
#include "textcache.h"
#include "theme.h"

static tc_cache cache;
static int loads_left;

static void free_icon(void *payload)
{
    GX_DrawDone();
    GRRLIB_FreeTexture((GRRLIB_texImg *)payload);
}

void icons_init(void)
{
    tc_init_capacity(&cache, free_icon, ICON_CACHE_CAPACITY);
}

void icons_frame(void)
{
    loads_left = ICON_LOADS_PER_FRAME;
}

static GRRLIB_texImg *lookup(const char *path)
{
    tc_entry *e = tc_find(&cache, 0, 0, 0, path);
    if (e != NULL) return (GRRLIB_texImg *)e->payload;
    if (loads_left <= 0) return NULL;
    loads_left--;
    GRRLIB_texImg *tex = ui_load_png_file(path);
    if (tc_insert(&cache, 0, 0, 0, path, tex) == NULL && tex != NULL) {
        free_icon(tex);
        return NULL;
    }
    return tex;
}

void icons_draw(const char *path, float x, float y, float size)
{
    float w = size * ui_pixel_x();
    float ix = x + (size - w) / 2.0f;
    GRRLIB_texImg *tex = path != NULL && path[0] != '\0' ? lookup(path) : NULL;
    if (tex == NULL) {
        ui_panel(ix, y, w, size, size * 0.22f, THEME_PLACEHOLDER_FILL, THEME_PANEL_BORDER);
        return;
    }
    ui_image(tex, ix, y, w, size, 1.0f);
}
