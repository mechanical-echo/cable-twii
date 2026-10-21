#include "input.h"

#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

static uint32_t rumble_until_ms = 0;

static uint32_t in_now_ms(void)
{
    return (uint32_t)(ticks_to_millisecs(gettime()));
}

void in_init(void)
{
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);
    WPAD_SetIdleTimeout(3600);
}

void in_poll(void)
{
    WPAD_ScanPads();
    if (rumble_until_ms != 0 && in_now_ms() >= rumble_until_ms) {
        WPAD_Rumble(WPAD_CHAN_0, 0);
        rumble_until_ms = 0;
    }
}

bool in_pressed(uint32_t wpad_button)
{
    return (WPAD_ButtonsDown(WPAD_CHAN_0) & wpad_button) != 0;
}

bool in_held(uint32_t wpad_button)
{
    return (WPAD_ButtonsHeld(WPAD_CHAN_0) & wpad_button) != 0;
}

bool in_ir(float *x, float *y, float *angle)
{
    ir_t ir;
    WPAD_IR(WPAD_CHAN_0, &ir);
    if (!ir.valid) return false;
    *x = ir.x;
    *y = ir.y;
    *angle = ir.angle;
    return true;
}

void in_rumble_pulse(uint32_t ms)
{
    WPAD_Rumble(WPAD_CHAN_0, 1);
    rumble_until_ms = in_now_ms() + ms;
}
