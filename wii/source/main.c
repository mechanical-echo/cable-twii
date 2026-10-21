#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>

#include "app.h"
#include "input.h"
#include "storage.h"
#include "ui/theme.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/cursor.h"
#include "ui/sfx.h"
#include "screens/error.h"
#include "media/player.h"

static app g_app;

void app_go(app *a, screen_id s)
{
    a->screen = s;
}

void app_fail(app *a, const char *title, const char *body)
{
    strncpy(a->error_title, title, sizeof(a->error_title) - 1);
    a->error_title[sizeof(a->error_title) - 1] = '\0';
    strncpy(a->error_body, body, sizeof(a->error_body) - 1);
    a->error_body[sizeof(a->error_body) - 1] = '\0';
    app_go(a, SCR_ERROR);
    error_enter(a);
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

static void boot(app *a)
{
    if (st_find_root(a->root, sizeof a->root) != 0) {
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
    app_go(a, SCR_LOGO);
}

static bool point_in(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

static void logo_placeholder_draw(app *a)
{
    (void)a;
    ui_stripes();
    ui_text(FONT_BOLD, 28, 320, 220, "cable-twii", THEME_ACCENT, ALIGN_CENTER);
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

    memset(&g_app, 0, sizeof g_app);
    boot(&g_app);

    bool was_hot = false;
    const float pill_x = 240.0f, pill_y = 220.0f, pill_w = 160.0f, pill_h = 44.0f;

    for (;;) {
        in_poll();
        cur_update();
        if (in_pressed(WPAD_BUTTON_HOME)) break;

        switch (g_app.screen) {
        case SCR_ERROR:
            error_update(&g_app);
            error_draw(&g_app);
            break;
        case SCR_LOGO:
        default: {
            bool hot = cur_visible() && point_in(cur_x(), cur_y(), pill_x, pill_y, pill_w, pill_h);
            if (hot && !was_hot) {
                sfx_play(SFX_HOVER);
                in_rumble_pulse(30);
            }
            was_hot = hot;

            if (in_pressed(WPAD_BUTTON_A)) pl_open("sd:/test.mpg", 0);
            if (in_pressed(WPAD_BUTTON_1)) pl_open("sd:/test.mpg", 5000);
            if (in_pressed(WPAD_BUTTON_B)) pl_close();
            if (in_pressed(WPAD_BUTTON_PLUS)) pl_set_volume(100);
            if (in_pressed(WPAD_BUTTON_MINUS)) pl_set_volume(30);

            if (pl_status() != PL_IDLE) {
                pl_draw(120, 60, 400, 225, 1.0f);
                char buf[32];
                snprintf(buf, sizeof buf, "%u ms", (unsigned)pl_position_ms());
                ui_text(FONT_BODY, 16, 120, 300, buf, THEME_TEXT_PRIMARY, ALIGN_LEFT);
            } else {
                logo_placeholder_draw(&g_app);
                ui_pill(pill_x, pill_y, pill_w, pill_h, g_app.root[0] != '\0' ? g_app.root : "no root", hot);
            }
            break;
        }
        }

        cur_draw();
        GRRLIB_Render();
    }

    GRRLIB_Exit();
    exit(0);
}
