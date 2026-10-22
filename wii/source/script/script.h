#ifndef CTW_DEBUG_SCRIPT_H
#define CTW_DEBUG_SCRIPT_H

#include <stdbool.h>
#include <stdint.h>

void dbg_script_load(const char *root);
void dbg_script_poll(void);
bool dbg_script_active(void);
uint32_t dbg_script_down(void);
uint32_t dbg_script_held(void);
uint32_t dbg_script_up(void);
bool dbg_script_ir(float *x, float *y);

#endif
