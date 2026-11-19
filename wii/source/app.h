#ifndef CTW_APP_H
#define CTW_APP_H

#include <stdbool.h>
#include <stdint.h>
#include "ctw/ctw.h"
#include "tuner.h"
#include "storage.h"
#include "quit.h"
#include "state.h"
#include "settings.h"
#include "net/addr.h"

typedef enum { SCR_LOGO, SCR_GUIDE, SCR_FULL, SCR_ERROR, SCR_SETTINGS } screen_id;

typedef struct {
    char root[256];
    char state_dir[64];
    st_volume drive;
    ctw_settings settings;
    bool settings_saveable;
    bool from_server;
    bool loading;
    bool failed;
    srv_addr server;
    char revision[64];
    ctw_db *db;
    tuner tuner;
    screen_id screen;
    screen_id prev_screen;
    bool home_open;
    bool today_open;
    int current_channel;
    int volume;
    ctw_state saved;
    uint32_t saved_ms;
    bool widescreen;
    uint32_t now_ms;
    ctw_date today;
    uint32_t ms_of_day;
    char error_title[64];
    char error_body[512];
    char notice[96];
    uint32_t notice_until_ms;
} app;

void app_go(app *a, screen_id s);
void app_fail(app *a, const char *title, const char *body);
void app_reload(app *a);
void app_open_settings(app *a);
void app_set_picture(app *a, asp_mode mode);
void app_switch_source(app *a, const ctw_settings *s);
void app_test_server(const srv_addr *addr);
void app_quit(app *a, quit_dest dest) __attribute__((noreturn));

#endif
