#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <grrlib.h>
#include <asndlib.h>
#include <wiiuse/wpad.h>

#include "app.h"
#include "input.h"
#include "storage.h"
#include "quit.h"
#include "state.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/cursor.h"
#include "ui/sfx.h"
#include "ui/icons.h"
#include "media/player.h"
#include "screens/error.h"
#include "screens/full.h"
#include "screens/guide.h"
#include "screens/homemenu.h"
#include "screens/logo.h"
#ifdef CTW_DEBUG_OVERLAY
#include "debug/overlay.h"
#endif

static app g_app;

void app_go(app *a, screen_id s)
{
    a->prev_screen = a->screen;
    a->screen = s;
    if (s == SCR_FULL) full_enter(a);
    if (s == SCR_GUIDE) guide_enter(a);
    if (s == SCR_LOGO) logo_enter(a);
}

void app_fail(app *a, const char *title, const char *body)
{
    strncpy(a->error_title, title, sizeof(a->error_title) - 1);
    a->error_title[sizeof(a->error_title) - 1] = '\0';
    strncpy(a->error_body, body, sizeof(a->error_body) - 1);
    a->error_body[sizeof(a->error_body) - 1] = '\0';
    a->screen = SCR_ERROR;
    error_enter(a);
}

static ctw_state current_state(const app *a)
{
    ctw_state s = { a->current_channel, a->volume };
    return s;
}

static bool can_save(const app *a)
{
    return a->drive.found && a->drive.writable && a->db != NULL && a->screen != SCR_ERROR;
}

static void save_state(app *a, bool force)
{
    if (!can_save(a)) return;
    ctw_state s = current_state(a);
    bool changed = state_changed(&s, &a->saved);
    if (force ? !changed : !state_save_due(changed, a->saved_ms, a->now_ms)) return;
    state_save(a->root, &s);
    a->saved = s;
    a->saved_ms = a->now_ms;
}

static bool running_in_emulator(void)
{
    s32 fd = IOS_Open("/dev/dolphin", 0);
    if (fd < 0) return false;
    IOS_Close(fd);
    return true;
}

void app_quit(app *a, quit_dest dest)
{
    save_state(a, true);
    bool emulator = running_in_emulator();
    pl_close();
    ASND_End();
    WPAD_Shutdown();
    st_release(&a->drive);
    GRRLIB_Exit();
    switch (quit_route_for(dest, quit_stub_present((const unsigned char *)0x80001800), emulator)) {
    case QUIT_POWER_OFF:
        SYS_ResetSystem(SYS_POWEROFF, 0, 0);
        break;
    case QUIT_WII_MENU:
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
        break;
    case QUIT_LOADER:
        break;
    }
    exit(0);
}

static void append_issue_line(char *out, size_t out_size, const ctw_issue *iss)
{
    size_t len = strlen(out);
    if (len >= out_size - 1) return;
    if (strcmp(iss->file, "config") == 0 && iss->line > 0) {
        snprintf(out + len, out_size - len, "config.toml line %d: %s\n", iss->line, iss->message);
    } else if (strcmp(iss->file, "library") == 0) {
        snprintf(out + len, out_size - len, "library.toml: %s\n", iss->message);
    } else {
        snprintf(out + len, out_size - len, "%s\n", iss->message);
    }
}

static int start_channel(const ctw_db *db, int last_channel)
{
    size_t count = ctw_db_channel_count(db);
    if (count == 0) return 0;
    int *numbers = malloc(count * sizeof *numbers);
    if (numbers == NULL) return ctw_db_channel_at(db, 0)->number;
    for (size_t i = 0; i < count; i++) numbers[i] = ctw_db_channel_at(db, i)->number;
    int channel = state_start_channel(last_channel, numbers, count);
    free(numbers);
    return channel;
}

static void boot(app *a)
{
    if (st_find_root(a->root, sizeof a->root, &a->drive) != 0) {
        app_fail(a, "No cable-twii drive found",
                  "Plug in the drive prepared with cable-twii Studio, or put a cable-twii folder on the SD card.");
        return;
    }

    ctw_issues issues;
    memset(&issues, 0, sizeof issues);
    int rc = ctw_db_load(a->root, &a->db, &issues);
    if (rc != 0) {
        char body[512] = "";
        size_t shown = 0;
        for (size_t i = 0; i < issues.count && shown < 3; i++) {
            if (issues.items[i].level != CTW_LEVEL_ERROR) continue;
            append_issue_line(body, sizeof body, &issues.items[i]);
            shown++;
        }
        size_t len = strlen(body);
        snprintf(body + len, sizeof body - len, "Open cable-twii Studio to fix it.");
        ctw_issues_free(&issues);
        ctw_db_free(a->db);
        a->db = NULL;
        app_fail(a, "There's a problem with the schedule", body);
        return;
    }
    ctw_issues_free(&issues);

    tn_init(&a->tuner, a->db);

    ctw_state saved;
    state_load(a->root, &saved);
    a->volume = saved.volume;
    a->current_channel = start_channel(a->db, saved.last_channel);
    a->saved = current_state(a);
    a->saved_ms = a->now_ms;
    guide_prepare(a);
    app_go(a, SCR_LOGO);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    GRRLIB_Init();
    in_init();
    ui_text_init();
    cur_init();
    sfx_init();
    icons_init();
#ifdef CTW_DEBUG_OVERLAY
    overlay_init();
#endif

    memset(&g_app, 0, sizeof g_app);
    g_app.volume = STATE_DEFAULT_VOLUME;
    g_app.widescreen = CONF_GetAspectRatio() == CONF_ASPECT_16_9;
    ui_set_widescreen(g_app.widescreen);
    pl_set_widescreen(g_app.widescreen);
    boot(&g_app);

    for (;;) {
        in_poll();
        cur_update();
        g_app.now_ms += 16;
        if (in_power_requested()) app_quit(&g_app, QUIT_DEST_POWER_OFF);
        if (in_reset_requested()) app_quit(&g_app, QUIT_DEST_HOMEBREW);

        bool was_open = g_app.home_open;
        home_update(&g_app);
        bool paused = was_open || g_app.home_open;
        in_block(paused);
        cur_block(paused);

        switch (g_app.screen) {
        case SCR_ERROR:
            error_update(&g_app);
            error_draw(&g_app);
            break;
        case SCR_FULL:
            full_update(&g_app);
            full_draw(&g_app);
            break;
        case SCR_LOGO:
            logo_update(&g_app);
            logo_draw(&g_app);
            break;
        case SCR_GUIDE:
        default:
            guide_update(&g_app);
            guide_draw(&g_app);
            break;
        }

        in_block(false);
        cur_block(false);
        save_state(&g_app, false);
        home_draw(&g_app);
#ifdef CTW_DEBUG_OVERLAY
        overlay_draw();
#endif

        if (paused || g_app.screen != SCR_FULL || full_cursor_visible(&g_app)) cur_draw();
        GRRLIB_Render();
    }
}
