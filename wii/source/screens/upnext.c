#include "upnext.h"

#include <stdio.h>
#include <string.h>
#include <grrlib.h>

#include "../guide_text.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/text.h"
#include "../ui/theme.h"

#define BLOOM_CX 320.0f
#define BLOOM_CY 216.0f
#define BLOOM_RX 384.0f
#define BLOOM_RY 288.0f
#define BLOOM_COLOR 0x34BEED29u
#define CARD_X 160.0f
#define CARD_Y 111.1f
#define CARD_W 320.0f
#define CARD_H 222.2f
#define CARD_SHADOW 0x00000080u
#define CARD_SHADOW_DY 8.0f
#define CARD_SHADOW_BLUR 22.0f
#define DOT_X 19.3f
#define DOT_Y 28.0f
#define DOT_H 8.0f
#define DOT_RING 2.7f
#define LABEL_X 30.7f
#define LABEL_Y 22.2f
#define LABEL_PX 18
#define ICON_X 19.3f
#define ICON_Y 55.1f
#define ICON_PX 53.0f
#define TITLE_X 68.7f
#define TITLE_Y 60.8f
#define TITLE_PX 20
#define SHOW_Y 86.8f
#define SHOW_PX 13
#define DIVIDER_Y 121.8f
#define AT_Y 142.9f
#define AT_PX 15
#define BAR_X 77.6f
#define BAR_Y 148.9f
#define BAR_W 156.3f
#define BAR_H 6.2f
#define RIGHT_X 300.7f
#define COUNT_Y 136.0f
#define COUNT_PX 32
#define PILL_X 220.0f
#define PILL_Y 364.4f
#define PILL_W 200.0f
#define PILL_H 35.6f
#define PILL_FILL 0xFFFFFF14u
#define PILL_BORDER 0xFFFFFF24u
#define PILL_DOT_X 33.9f
#define PILL_DOT_Y 14.3f
#define PILL_DOT_H 7.1f
#define PILL_TEXT_X 44.6f
#define PILL_TEXT_Y 10.4f
#define PILL_PX 12

typedef struct {
    bool valid;
    int channel;
    const ctw_airing *episode;
    int64_t start;
    char title[TF_LINE_BYTES];
    char show[TF_LINE_BYTES];
    char at[16];
} upnext_text;

static upnext_text text;

static void refresh(const ctw_airing *ep)
{
    memset(&text, 0, sizeof text);
    text.valid = true;
    text.episode = ep;
    if (ep == NULL || ep->episode == NULL) return;
    text.start = ep->start_ms;
    char buf[TF_LINE_BYTES];
    gt_episode(ep->episode, buf, sizeof buf);
    ui_text_fit(FONT_BOLD, TITLE_PX, buf, RIGHT_X - TITLE_X, text.title, sizeof text.title);
    ui_text_fit(FONT_BODY, SHOW_PX, ep->show != NULL ? ep->show->name : "", RIGHT_X - TITLE_X, text.show, sizeof text.show);
    gt_at(ep->start_ms, text.at, sizeof text.at);
}

static void draw_break_pill(void)
{
    ui_rounded(PILL_X, PILL_Y, PILL_W, PILL_H, PILL_H / 2.0f, PILL_FILL);
    ui_outline(PILL_X, PILL_Y, PILL_W, PILL_H, PILL_H / 2.0f, 2.0f, PILL_BORDER);
    float dw = PILL_DOT_H * ui_pixel_x();
    ui_rounded(PILL_X + PILL_DOT_X, PILL_Y + PILL_DOT_Y, dw, PILL_DOT_H, PILL_DOT_H / 2.0f, THEME_WARM);
    ui_text(FONT_BOLD, PILL_PX, PILL_X + PILL_TEXT_X, PILL_Y + PILL_TEXT_Y, "Up next: Commercial break", THEME_TEXT_FADED, ALIGN_LEFT);
}

void upnext_draw(app *a, const ctw_airing *gap, uint32_t gap_offset_ms, const ctw_airing *next, const ctw_airing *next_episode, int64_t ms_until_episode)
{
    (void)gap;
    ui_stripes_colors(THEME_STRIPE_DARK_A, THEME_STRIPE_DARK_B);
    ui_stripes();
    ui_stripes_colors(THEME_STRIPE_A, THEME_STRIPE_B);
    ui_radial(BLOOM_CX, BLOOM_CY, BLOOM_RX, BLOOM_RY, BLOOM_COLOR);

    ui_shadow(CARD_X, CARD_Y, CARD_W, CARD_H, THEME_PANEL_RADIUS, CARD_SHADOW, CARD_SHADOW_DY, CARD_SHADOW_BLUR);
    ui_ring(CARD_X, CARD_Y, CARD_W, CARD_H, THEME_PANEL_RADIUS, THEME_RING, THEME_RING_SPREAD);
    ui_card(CARD_X, CARD_Y, CARD_W, CARD_H, THEME_PANEL_RADIUS, 0.55f, THEME_GLOSS_SOFT, UI_SHADOW_NONE);

    float dw = DOT_H * ui_pixel_x();
    ui_ring(CARD_X + DOT_X, CARD_Y + DOT_Y, dw, DOT_H, DOT_H / 2.0f, THEME_RING, DOT_RING);
    ui_rounded(CARD_X + DOT_X, CARD_Y + DOT_Y, dw, DOT_H, DOT_H / 2.0f, THEME_ACCENT);
    ui_text(FONT_BOLD, LABEL_PX, CARD_X + LABEL_X, CARD_Y + LABEL_Y, "Up next", THEME_ACCENT, ALIGN_LEFT);

    if (!text.valid || text.channel != a->current_channel || text.episode != next_episode ||
        (next_episode != NULL && text.start != next_episode->start_ms)) {
        refresh(next_episode);
        text.channel = a->current_channel;
    }

    const ctw_channel *ch = ctw_db_channel(a->db, a->current_channel);
    char path[320] = "";
    if (ch != NULL) gt_icon_path(a->root, ch, next_episode != NULL ? next_episode->show : NULL, path, sizeof path);
    float iw = ICON_PX * ui_pixel_x();
    icons_draw(path, CARD_X + ICON_X - (ICON_PX - iw) / 2.0f, CARD_Y + ICON_Y, ICON_PX);
    ui_text(FONT_BOLD, TITLE_PX, CARD_X + TITLE_X, CARD_Y + TITLE_Y, text.title, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    ui_text(FONT_BODY, SHOW_PX, CARD_X + TITLE_X, CARD_Y + SHOW_Y, text.show, THEME_TEXT_SECONDARY, ALIGN_LEFT);
    GRRLIB_Rectangle(CARD_X + DOT_X, CARD_Y + DIVIDER_Y, RIGHT_X - DOT_X, 1.0f, THEME_BAR_TRACK, 1);

    if (next_episode != NULL) ui_text(FONT_BOLD, AT_PX, CARD_X + DOT_X, CARD_Y + AT_Y, text.at, THEME_TEXT_PRIMARY, ALIGN_LEFT);
    float total = (float)gap_offset_ms + (float)(ms_until_episode > 0 ? ms_until_episode : 0);
    ui_bar(CARD_X + BAR_X, CARD_Y + BAR_Y, BAR_W, BAR_H, total > 0.0f ? (float)gap_offset_ms / total : 0.0f);
    char countdown[16];
    gt_countdown((uint32_t)(ms_until_episode < 0 ? 0 : (ms_until_episode > 0xFFFFFFFFLL ? 0xFFFFFFFFu : ms_until_episode)), countdown, sizeof countdown);
    ui_text(FONT_BOLD, COUNT_PX, CARD_X + RIGHT_X, CARD_Y + COUNT_Y, countdown, THEME_ACCENT, ALIGN_RIGHT);

    if (next != NULL && next->kind == CTW_AIR_AD) draw_break_pill();
}
