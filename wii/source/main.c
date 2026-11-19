#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <grrlib.h>
#include <asndlib.h>
#include <wiiuse/wpad.h>

#include "app.h"
#include "input.h"
#include "ldqueue.h"
#include "live.h"
#include "loader.h"
#include "storage.h"
#include "quit.h"
#include "srcpick.h"
#include "state.h"
#include "settings.h"
#include "video.h"
#include "net/addr.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/cursor.h"
#include "ui/sfx.h"
#include "ui/icons.h"
#include "ui/glyphs.h"
#include "media/player.h"
#include "screens/error.h"
#include "screens/full.h"
#include "screens/guide.h"
#include "screens/homemenu.h"
#include "screens/logo.h"
#include "screens/notice.h"
#ifdef CTW_DEBUG_OVERLAY
#include "debug/overlay.h"
#endif
#ifdef CTW_DEBUG_SCRIPT
#include "script/script.h"
#endif

static app g_app;
static lq_state jobs;
static uint32_t seen_day;
static bool reload_manual;

void app_go(app *a, screen_id s)
{
    a->prev_screen = a->screen;
    a->screen = s;
    if (s == SCR_FULL) full_enter(a);
    if (s == SCR_GUIDE) guide_enter(a);
    if (s == SCR_LOGO) logo_enter(a);
    if (s == SCR_GUIDE && a->from_server && a->prev_screen != SCR_GUIDE && a->prev_screen != SCR_LOGO) lq_want(&jobs, LQ_CHECK);
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
    if (a->db == NULL || a->screen == SCR_ERROR || a->state_dir[0] == '\0') return false;
    if (strcmp(a->state_dir, SETTINGS_DIR) == 0) return st_config_writable();
    return a->drive.found && a->drive.writable;
}

static void save_state(app *a, bool force)
{
    if (!can_save(a)) return;
    ctw_state s = current_state(a);
    bool changed = state_changed(&s, &a->saved);
    if (force ? !changed : !state_save_due(changed, a->saved_ms, a->now_ms)) return;
    state_save(a->state_dir, &s);
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
    icons_stop();
    ASND_End();
    WPAD_Shutdown();
    st_release(&a->drive);
    st_config_release();
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

static void start_guide(app *a, bool keep)
{
    tn_init(&a->tuner, a->db);

    ctw_state saved;
    state_load(a->state_dir, &saved);
    if (!keep) a->volume = saved.volume;
    a->current_channel = start_channel(a->db, keep ? a->current_channel : saved.last_channel);
    a->saved = keep ? saved : current_state(a);
    a->saved_ms = a->now_ms;
    guide_prepare(a);
}

static void drop_source(app *a)
{
    save_state(a, true);
    live_stop();
    guide_reset();
    full_reset();
    tn_free(&a->tuner);
    ctw_db_free(a->db);
    a->db = NULL;
    if (a->drive.found) st_release(&a->drive);
    a->from_server = false;
    a->revision[0] = '\0';
    a->notice[0] = '\0';
    a->notice_until_ms = 0;
}

static void reenter(app *a)
{
    if (a->screen == SCR_LOGO) return;
    app_go(a, a->screen == SCR_FULL && ctw_db_channel_count(a->db) > 0 ? SCR_FULL : SCR_GUIDE);
}

static void reload_notice(app *a, srv_result r, bool manual)
{
    const char *text = sp_reload_notice(r, manual);
    if (text != NULL) notice_show(a, text);
}

static bool load_drive(app *a, bool keep)
{
    int n = snprintf(a->state_dir, sizeof a->state_dir, "%s", a->root);
    if (n < 0 || (size_t)n >= sizeof a->state_dir) a->state_dir[0] = '\0';

#ifdef CTW_DEBUG_SCRIPT
    if (!dbg_script_active()) dbg_script_load(a->root);
#endif
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
        return false;
    }
    ctw_issues_free(&issues);
    start_guide(a, keep);
    return true;
}

static bool open_drive(app *a, bool keep)
{
    if (st_find_root(a->root, sizeof a->root, &a->drive) != 0) {
        app_fail(a, "No cable-twii drive found",
                  "Plug in the drive prepared with cable-twii Studio, or put a cable-twii folder on the SD card.");
        return false;
    }
    return load_drive(a, keep);
}

static void start_server(app *a, ctw_db *db, const srv_info *info, bool keep)
{
    a->db = db;
    a->from_server = true;
    srv_root(&a->server, a->root, sizeof a->root);
    snprintf(a->state_dir, sizeof a->state_dir, "%s", SETTINGS_DIR);
    snprintf(a->revision, sizeof a->revision, "%s", info->revision);
    start_guide(a, keep);
}

static void fail_server(app *a, srv_result r)
{
    char address[sizeof a->server.host + 8];
    char body[sizeof a->error_body];
    addr_format(&a->server, address, sizeof address);
    sp_fail_body(r, address, body, sizeof body);
    app_fail(a, "Can't reach the server", body);
}

static void finish_loading(app *a)
{
    ctw_db *db;
    ctw_issues issues;
    srv_info info;
    srv_result r = ld_take_db(&db, &issues, &info);
    ctw_issues_free(&issues);
    a->loading = false;
    bool reload = a->db != NULL;
    bool drive_found = !reload && r != SRV_OK && st_find_root(a->root, sizeof a->root, &a->drive) == 0;
    switch (sp_after_server(r, reload, drive_found)) {
    case SP_SERVER:
        if (reload) drop_source(a);
        start_server(a, db, &info, reload);
        if (reload) reenter(a);
        break;
    case SP_KEEP:
        ctw_db_free(db);
        break;
    case SP_DRIVE_WITH_NOTICE:
        if (load_drive(a, false)) notice_show(a, sp_fallback_notice(a->drive.source));
        break;
    case SP_FAIL:
        fail_server(a, r);
        break;
    }
    if (reload) reload_notice(a, r, reload_manual);
}

static void want_server(app *a)
{
    addr_parse(a->settings.server, &a->server);
    a->loading = true;
    lq_want(&jobs, LQ_LOAD);
}

static void boot_source(app *a)
{
    if (sp_try_server(&a->settings)) {
#ifdef CTW_DEBUG_SCRIPT
        if (!dbg_script_active()) dbg_script_load(SETTINGS_DIR);
#endif
        want_server(a);
        app_go(a, SCR_LOGO);
        return;
    }
    if (open_drive(a, false)) app_go(a, SCR_LOGO);
}

static void connecting_notice(app *a)
{
    char address[sizeof a->server.host + 8];
    char text[sizeof a->notice];
    addr_format(&a->server, address, sizeof address);
    sp_connecting(address, text, sizeof text);
    notice_show(a, text);
}

static void start_reload(app *a, bool manual)
{
    lq_reset(&jobs);
    a->loading = false;
    if (a->db == NULL || a->screen == SCR_ERROR) {
        drop_source(a);
        boot_source(a);
        return;
    }
    if (sp_try_server(&a->settings)) {
        reload_manual = manual;
        want_server(a);
        if (manual) connecting_notice(a);
        return;
    }
    drop_source(a);
    if (!open_drive(a, true)) return;
    reload_notice(a, SRV_OK, manual);
    reenter(a);
}

void app_reload(app *a)
{
    if (a->loading) return;
    start_reload(a, true);
}

static void finish_check(app *a)
{
    srv_info info;
    if (ld_take_info(&info) == SRV_OK && a->from_server && strcmp(info.revision, a->revision) != 0) start_reload(a, false);
}

static void finish_probe(app *a)
{
    srv_info info;
    live_probe_done(a, ld_take_info(&info) == SRV_OK);
}

static void watch_day(app *a)
{
    uint32_t day = a->from_server && a->db != NULL ? ctw_date_key(a->today) : 0;
    if (lq_new_day(&seen_day, day)) lq_want(&jobs, LQ_CHECK);
}

static void loader_update(app *a)
{
    watch_day(a);
    if (ld_poll() == LD_DONE) {
        switch (lq_finish(&jobs)) {
        case LQ_LOAD:
            finish_loading(a);
            break;
        case LQ_CHECK:
            finish_check(a);
            break;
        case LQ_PROBE:
            finish_probe(a);
            break;
        case LQ_NONE:
            break;
        }
        ld_drop();
    }
    switch (lq_start(&jobs, ld_poll() == LD_RUNNING, live_probe_due(a))) {
    case LQ_LOAD:
        ld_load(&a->server);
        break;
    case LQ_PROBE:
        live_probe_sent(a);
        ld_info(&a->server);
        break;
    case LQ_CHECK:
        ld_info(&a->server);
        break;
    case LQ_NONE:
        break;
    }
}

static void boot(app *a)
{
    st_config_mount();
    settings_load(SETTINGS_DIR, &a->settings);
    a->settings_saveable = st_config_writable();
    boot_source(a);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    video_init();
    in_init();
    ui_text_init();
    cur_init();
    sfx_init();
    icons_init();
    glyphs_init();
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
        loader_update(&g_app);

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
