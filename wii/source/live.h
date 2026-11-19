#ifndef CTW_LIVE_H
#define CTW_LIVE_H

#include <stdbool.h>
#include <stdint.h>
#include "app.h"

void live_tune(app *a, int channel);
void live_stop(void);
bool live_update(app *a);
bool live_on(int channel);
bool live_probe_due(const app *a);
void live_probe_sent(const app *a);
void live_probe_done(app *a, bool answered);
bool live_weak(void);
float live_fade_in(const app *a, uint32_t fade_ms);

#endif
