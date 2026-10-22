#ifndef CTW_INPUT_H
#define CTW_INPUT_H

#include <stdbool.h>
#include <stdint.h>

void in_init(void);
void in_poll(void);
bool in_pressed(uint32_t wpad_button);
bool in_held(uint32_t wpad_button);
bool in_ir(float *x, float *y, float *angle);
void in_rumble_pulse(uint32_t ms);
void in_block(bool on);
bool in_power_requested(void);
bool in_reset_requested(void);
uint8_t in_battery_level(void);

#endif
