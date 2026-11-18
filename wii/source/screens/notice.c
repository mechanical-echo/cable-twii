#include "notice.h"

#include <stdio.h>

#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define NOTICE_MS 4000u
#define NOTICE_PX 13
#define NOTICE_Y 10.0f
#define NOTICE_H 28.4f
#define NOTICE_PAD 28.0f
#define SCREEN_W 640.0f

void notice_show(app *a, const char *text)
{
    snprintf(a->notice, sizeof a->notice, "%s", text);
    a->notice_until_ms = a->now_ms + NOTICE_MS;
}

void notice_restart(app *a)
{
    if (a->now_ms < a->notice_until_ms) a->notice_until_ms = a->now_ms + NOTICE_MS;
}

static void pill(const char *text, float cx, float y, ui_shadow_kind shadow)
{
    float w = ui_text_width(FONT_BOLD, NOTICE_PX, text) + NOTICE_PAD;
    ui_pill_ex(cx - w / 2.0f, y, w, NOTICE_H, NULL, NOTICE_PX, UI_PILL_REST, shadow);
    ui_text(FONT_BOLD, NOTICE_PX, cx, y + (NOTICE_H - (float)NOTICE_PX) / 2.0f, text, THEME_TEXT_PRIMARY, ALIGN_CENTER);
}

void notice_draw(const app *a)
{
    if (a->now_ms >= a->notice_until_ms) return;
    pill(a->notice, SCREEN_W / 2.0f, NOTICE_Y, a->screen == SCR_FULL ? UI_SHADOW_VIDEO : UI_SHADOW_PANEL);
}

void notice_weak(asp_rect video)
{
    pill("Weak signal", video.x + video.w / 2.0f, video.y + (video.h - NOTICE_H) / 2.0f, UI_SHADOW_VIDEO);
}
