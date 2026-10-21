#ifndef CTW_APP_H
#define CTW_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "ctw/ctw.h"
#include "tuner.h"

typedef enum { SCR_LOGO, SCR_GUIDE, SCR_FULL, SCR_ERROR } screen_id;

typedef struct {
    char root[256];
    ctw_db *db;
    tuner tuner;
    screen_id screen;
    bool home_open;
    int current_channel;
    int volume;
    bool widescreen;
    uint32_t now_ms;
    ctw_date today;
    uint32_t ms_of_day;
    char error_title[64];
    char error_body[512];
} app;

void app_go(app *a, screen_id s);
void app_fail(app *a, const char *title, const char *body);

#endif
