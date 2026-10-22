#include "guide.h"

#include <stdlib.h>
#include <wiiuse/wpad.h>

#include "../clock.h"
#include "../guide_layout.h"
#include "../guide_model.h"
#include "../guide_text.h"
#include "../input.h"
#include "../live.h"
#include "../logoanim.h"
#include "../preview.h"
#include "../ui/cursor.h"
#include "../ui/draw.h"
#include "../ui/icons.h"
#include "../ui/sfx.h"
#include "../ui/text.h"
#include "../ui/theme.h"
#include "../ui/tween.h"
#include "guide_info.h"
#include "guide_list.h"
#include "logo.h"

#define SCROLL_MS 200u
#define CLOCK_PX 20
#define FADE_MARGIN 8.0f

typedef enum { G_IDLE, G_EXPAND, G_RETURN } g_mode;

static gm_row *rows;
static size_t row_count;
static bool built;
static guide_layout layout;
static int highlight = -1;
static int last_hit = -1;
static float scroll;
static tween scroll_tw;
static bool scrolling;
static pv_state pv;
static g_mode mode;
static uint32_t anim_start;

static void ensure_model(app *a)
{
    if (built) return;
    built = true;
    layout = guide_layout_for(ui_pixel_x());
    size_t count = a->db != NULL ? ctw_db_channel_count(a->db) : 0;
    rows = count > 0 ? (gm_row *)malloc(2 * count * sizeof *rows) : NULL;
    row_count = rows != NULL ? gm_build(a->db, rows, 2 * count) : 0;
    if (glist_init(row_count) != 0) row_count = 0;
}

void guide_prepare(app *a)
{
    ensure_model(a);
    clk_now(&a->today, &a->ms_of_day);
    for (size_t i = 0; i < row_count; i++) {
        if (rows[i].kind != GM_CHANNEL) continue;
        uint32_t offset = 0;
        tn_current(&a->tuner, rows[i].channel->number, a->today, a->ms_of_day, &offset);
    }
}

static float view_top(void)
{
    float top, h;
    glist_view(layout.list, &top, &h);
    return top;
}

static float view_h(void)
{
    float top, h;
    glist_view(layout.list, &top, &h);
    return h;
}

static float clamp_scroll(float s)
{
    float max = gm_content_h(rows, row_count) - view_h();
    if (s > max) s = max;
    if (s < 0.0f) s = 0.0f;
    return s;
}

static int pointer_row(void)
{
    if (!cur_visible()) return -1;
    float px = cur_x(), py = cur_y();
    if (px < layout.list.x || px > layout.list.x + layout.list.w) return -1;
    if (py < view_top() || py > view_top() + view_h()) return -1;
    return gm_hit(rows, row_count, scroll, view_top(), px, py);
}

static int highlighted_channel(void)
{
    if (highlight < 0 || (size_t)highlight >= row_count) return 0;
    return rows[highlight].channel->number;
}

void guide_enter(app *a)
{
    ensure_model(a);
    highlight = gm_find(rows, row_count, a->current_channel);
    if (highlight < 0) highlight = gm_step(rows, row_count, -1, 1);
    scroll = clamp_scroll(gm_scroll_to(rows, row_count, highlight, scroll, view_h()));
    scrolling = false;
    last_hit = pointer_row();
    anim_start = a->now_ms;
    if (a->prev_screen == SCR_FULL && live_on(a->current_channel)) {
        mode = G_RETURN;
        pv_force(&pv, a->current_channel);
    } else {
        mode = G_IDLE;
        pv_reset(&pv, live_on(a->current_channel) ? a->current_channel : 0);
    }
}

static void move_highlight(app *a, int dir)
{
    int next = gm_step(rows, row_count, highlight, dir);
    if (next == highlight || next < 0) return;
    highlight = next;
    sfx_play(SFX_HOVER);
    int anchor = dir < 0 && next > 0 && rows[next - 1].kind == GM_HEADER ? next - 1 : next;
    float target = gm_scroll_to(rows, row_count, anchor, scroll, view_h());
    target = gm_scroll_to(rows, row_count, next, target, view_h());
    if (target == scroll) return;
    scroll_tw = tw_make(scroll, clamp_scroll(target), a->now_ms, SCROLL_MS);
    scrolling = true;
}

static bool in_rect(asp_rect r, float px, float py)
{
    return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h;
}

static void select_row(app *a, int index)
{
    if (index < 0 || (size_t)index >= row_count || rows[index].kind != GM_CHANNEL) return;
    highlight = index;
    int channel = rows[index].channel->number;
    sfx_play(SFX_SELECT);
    if (!live_on(channel)) live_tune(a, channel);
    pv_force(&pv, channel);
    mode = G_EXPAND;
    anim_start = a->now_ms;
}

static void handle_input(app *a)
{
    if (scrolling) {
        scroll = tw_value(&scroll_tw, a->now_ms);
        if (tw_done(&scroll_tw, a->now_ms)) scrolling = false;
    }
    if (in_pressed(WPAD_BUTTON_DOWN)) move_highlight(a, 1);
    else if (in_pressed(WPAD_BUTTON_UP)) move_highlight(a, -1);

    int hit = -1;
    if (cur_visible()) {
        float px = cur_x(), py = cur_y();
        if (px >= layout.list.x && px <= layout.list.x + layout.list.w) {
            float next = gm_edge_scroll(scroll, py, view_top(), view_h(), gm_content_h(rows, row_count));
            if (next != scroll) {
                scroll = next;
                scrolling = false;
            }
        }
        hit = pointer_row();
        if (hit >= 0 && hit != last_hit && hit != highlight) {
            highlight = hit;
            sfx_play(SFX_HOVER);
            in_rumble_pulse(THEME_HOVER_RUMBLE_MS);
        }
    }
    last_hit = hit;

    if (!in_pressed(WPAD_BUTTON_A)) return;
    if (!cur_visible()) select_row(a, highlight);
    else if (hit >= 0) select_row(a, hit);
    else if (in_rect(layout.preview, cur_x(), cur_y())) select_row(a, highlight);
}

void guide_update(app *a)
{
    if (!live_update(a)) return;
    uint32_t elapsed = a->now_ms - anim_start;
    if (mode == G_EXPAND) {
        if (elapsed >= THEME_PREVIEW_EXPAND_MS) app_go(a, SCR_FULL);
        return;
    }
    if (mode == G_RETURN) {
        if (elapsed < THEME_PREVIEW_EXPAND_MS) return;
        mode = G_IDLE;
    }
    handle_input(a);
    if (mode != G_IDLE) return;
    int channel = pv_update(&pv, highlighted_channel(), a->now_ms);
    if (channel != 0) live_tune(a, channel);
}

static asp_rect shifted(asp_rect r, float dx)
{
    r.x += dx;
    return r;
}

static void draw_body(app *a, float progress)
{
    float slide = LOGO_PANEL_SLIDE_PX * (1.0f - progress);
    float fade = 1.0f - progress;

    asp_rect list = shifted(layout.list, -slide);
    glist_draw(a, rows, row_count, highlight, scroll, list);
    ui_stripes_rect(0.0f, list.y - FADE_MARGIN, list.x + list.w + FADE_MARGIN, list.h + 2.0f * FADE_MARGIN, fade);

    asp_rect preview = shifted(layout.preview, slide);
    asp_rect info = shifted(layout.info, slide);
    if (mode == G_IDLE) ginfo_draw_preview(a, preview, 0.0f);
    ginfo_draw_info(a, highlight >= 0 && (size_t)highlight < row_count ? rows[highlight].channel : NULL, info);

    char clock[8];
    gt_hhmm(a->ms_of_day, clock, sizeof clock);
    float right = layout.info.x + layout.info.w + slide;
    ui_text(FONT_BOLD, CLOCK_PX, right, (GL_HEADER_H - (float)CLOCK_PX) / 2.0f - 2.0f, clock, THEME_TEXT_PRIMARY, ALIGN_RIGHT);
    float fx = info.x - FADE_MARGIN;
    ui_stripes_rect(fx, 0.0f, 640.0f - fx, 480.0f, fade);
}

static asp_rect lerp_rect(asp_rect from, asp_rect to, float k)
{
    asp_rect r = {
        from.x + (to.x - from.x) * k,
        from.y + (to.y - from.y) * k,
        from.w + (to.w - from.w) * k,
        from.h + (to.h - from.h) * k,
    };
    return r;
}

static void draw_transition(app *a)
{
    float t = (float)(a->now_ms - anim_start) / (float)THEME_PREVIEW_EXPAND_MS;
    float k = tw_ease_out(t);
    if (mode == G_RETURN) k = 1.0f - k;
    asp_rect full = { 0.0f, 0.0f, 640.0f, 480.0f };
    ginfo_draw_preview(a, lerp_rect(layout.preview, full, k), k);
}

void guide_draw(app *a)
{
    icons_frame();
    ui_stripes();
    logo_draw_header();
    draw_body(a, 1.0f);
    if (mode != G_IDLE) draw_transition(a);
}

void guide_draw_intro(app *a, float progress)
{
    ensure_model(a);
    icons_frame();
    draw_body(a, progress);
}
