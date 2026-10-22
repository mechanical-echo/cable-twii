#include "input.h"

#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>
#ifdef CTW_DEBUG_SCRIPT
#include "script/script.h"
#endif

static uint32_t rumble_until_ms = 0;
static bool blocked;
static volatile bool power_flag;
static volatile bool reset_flag;

static uint32_t in_now_ms(void)
{
    return (uint32_t)(ticks_to_millisecs(gettime()));
}

static void on_console_power(void)
{
    power_flag = true;
}

static void on_remote_power(s32 chan)
{
    (void)chan;
    power_flag = true;
}

static void on_console_reset(u32 irq, void *ctx)
{
    (void)irq;
    (void)ctx;
    reset_flag = true;
}

void in_init(void)
{
    SYS_SetPowerCallback(on_console_power);
    SYS_SetResetCallback(on_console_reset);
    WPAD_Init();
    WPAD_SetPowerButtonCallback(on_remote_power);
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);
    WPAD_SetIdleTimeout(3600);
}

void in_poll(void)
{
    WPAD_ScanPads();
#ifdef CTW_DEBUG_SCRIPT
    dbg_script_poll();
#endif
    if (rumble_until_ms != 0 && in_now_ms() >= rumble_until_ms) {
        WPAD_Rumble(WPAD_CHAN_0, 0);
        rumble_until_ms = 0;
    }
}

void in_block(bool on)
{
    blocked = on;
}

bool in_power_requested(void)
{
    return power_flag;
}

bool in_reset_requested(void)
{
    return reset_flag;
}

uint8_t in_battery_level(void)
{
    return WPAD_BatteryLevel(WPAD_CHAN_0);
}

static uint32_t scripted_down(void)
{
#ifdef CTW_DEBUG_SCRIPT
    return dbg_script_down();
#else
    return 0;
#endif
}

static uint32_t scripted_held(void)
{
#ifdef CTW_DEBUG_SCRIPT
    return dbg_script_held();
#else
    return 0;
#endif
}

static uint32_t scripted_up(void)
{
#ifdef CTW_DEBUG_SCRIPT
    return dbg_script_up();
#else
    return 0;
#endif
}

bool in_pressed(uint32_t wpad_button)
{
    if (blocked) return false;
    return ((WPAD_ButtonsDown(WPAD_CHAN_0) | scripted_down()) & wpad_button) != 0;
}

bool in_held(uint32_t wpad_button)
{
    if (blocked) return false;
    return ((WPAD_ButtonsHeld(WPAD_CHAN_0) | scripted_held()) & wpad_button) != 0;
}

bool in_released(uint32_t wpad_button)
{
    if (blocked) return false;
    return ((WPAD_ButtonsUp(WPAD_CHAN_0) | scripted_up()) & wpad_button) != 0;
}

bool in_ir(float *x, float *y, float *angle)
{
#ifdef CTW_DEBUG_SCRIPT
    if (dbg_script_active()) {
        *angle = 0.0f;
        return dbg_script_ir(x, y);
    }
#endif
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
