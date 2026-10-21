#include <stdlib.h>
#include <string.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "input.h"
#include "ui/theme.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/cursor.h"
#include "ui/sfx.h"

static bool point_in(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

int main(int argc, char **argv)
{
    GRRLIB_Init();
    in_init();
    ui_text_init();
    cur_init();
    sfx_init();

    bool was_hot = false;
    const float pill_x = 240.0f, pill_y = 220.0f, pill_w = 160.0f, pill_h = 44.0f;

    for (;;) {
        in_poll();
        cur_update();
        if (in_pressed(WPAD_BUTTON_HOME)) break;

        bool hot = cur_visible() && point_in(cur_x(), cur_y(), pill_x, pill_y, pill_w, pill_h);
        if (hot && !was_hot) {
            sfx_play(SFX_HOVER);
            in_rumble_pulse(30);
        }
        was_hot = hot;

        ui_stripes();
        ui_panel(120, 80, 400, 100, THEME_PANEL_RADIUS, THEME_PANEL_FILL, THEME_PANEL_BORDER);
        ui_pill(pill_x, pill_y, pill_w, pill_h, "Adventure Time", hot);

        char fitted[64];
        ui_text_fit(FONT_BODY, 16, "The Amazing World of a Very Long Channel Name", 380.0f, fitted, sizeof fitted);
        ui_text(FONT_BODY, 16, 140, 120, fitted, THEME_TEXT_PRIMARY, ALIGN_LEFT);

        cur_draw();
        GRRLIB_Render();
    }

    GRRLIB_Exit();
    exit(0);
}
