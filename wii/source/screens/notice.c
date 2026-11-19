#include "notice.h"

#include <stdio.h>

#include "../notice_layout.h"
#include "../ui/draw.h"
#include "../ui/text.h"
#include "../ui/theme.h"
#include "logo.h"

#define NOTICE_MS 4000u
#define NOTICE_PX 13
#define NOTICE_H 28.4f
#define NOTICE_PAD 28.0f

void notice_show(app *a, const char *text)
{
    snprintf(a->notice, sizeof a->notice, "%s", text);
    a->notice_until_ms = a->now_ms + NOTICE_MS;
}

void notice_restart(app *a)
{
    if (a->now_ms < a->notice_until_ms) a->notice_until_ms = a->now_ms + NOTICE_MS;
}

static void pill_at(const char *text, asp_rect r, ui_shadow_kind shadow)
{
    ui_pill_ex(r.x, r.y, r.w, r.h, NULL, NOTICE_PX, UI_PILL_REST, shadow);
    ui_text(FONT_BOLD, NOTICE_PX, r.x + r.w / 2.0f, r.y + (r.h - (float)NOTICE_PX) / 2.0f, text, THEME_TEXT_PRIMARY, ALIGN_CENTER);
}

static float pill_w(const char *text)
{
    return ui_text_width(FONT_BOLD, NOTICE_PX, text) + NOTICE_PAD;
}

void notice_draw(const app *a)
{
    if (a->now_ms >= a->notice_until_ms) return;
    asp_rect logo;
    logo_header_rect(&logo.x, &logo.y, &logo.w, &logo.h);
    pill_at(a->notice, notice_pill_rect(pill_w(a->notice), logo, ui_pixel_x()), a->screen == SCR_FULL ? UI_SHADOW_VIDEO : UI_SHADOW_PANEL);
}

void notice_weak(asp_rect video)
{
    float w = pill_w("Weak signal");
    pill_at("Weak signal", (asp_rect){ video.x + (video.w - w) / 2.0f, video.y + (video.h - NOTICE_H) / 2.0f, w, NOTICE_H }, UI_SHADOW_VIDEO);
}
