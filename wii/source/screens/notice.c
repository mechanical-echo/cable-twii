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

void notice_draw(const app *a)
{
    if (a->now_ms >= a->notice_until_ms) return;
    float w = ui_text_width(FONT_BOLD, NOTICE_PX, a->notice) + NOTICE_PAD;
    float x = (SCREEN_W - w) / 2.0f;
    ui_shadow_kind shadow = a->screen == SCR_FULL ? UI_SHADOW_VIDEO : UI_SHADOW_PANEL;
    ui_pill_ex(x, NOTICE_Y, w, NOTICE_H, NULL, NOTICE_PX, UI_PILL_REST, shadow);
    ui_text(FONT_BOLD, NOTICE_PX, SCREEN_W / 2.0f, NOTICE_Y + (NOTICE_H - (float)NOTICE_PX) / 2.0f, a->notice, THEME_TEXT_PRIMARY, ALIGN_CENTER);
}
