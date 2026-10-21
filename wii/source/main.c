#include <stdlib.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "input.h"
#include "ui/theme.h"

static void draw_stripes(void)
{
    int y = 0;
    bool a = true;
    while (y < 480) {
        uint32_t color = a ? THEME_STRIPE_A : THEME_STRIPE_B;
        GRRLIB_Rectangle(0, y, 640, THEME_STRIPE_PX, color, 1);
        y += THEME_STRIPE_PX;
        a = !a;
    }
}

int main(int argc, char **argv)
{
    GRRLIB_Init();
    in_init();

    for (;;) {
        in_poll();
        if (in_pressed(WPAD_BUTTON_HOME)) break;

        draw_stripes();
        GRRLIB_Render();
    }

    GRRLIB_Exit();
    exit(0);
}
