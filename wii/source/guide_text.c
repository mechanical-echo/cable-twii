#include "guide_text.h"

#include <stdio.h>

#define DAY_MS ((int64_t)CTW_DAY_MS)
#define EN_DASH "\xE2\x80\x93"
#define MIDDOT "\xC2\xB7"

void gt_hhmm(int64_t ms, char *out, size_t out_size)
{
    int64_t m = ms % DAY_MS;
    if (m < 0) m += DAY_MS;
    snprintf(out, out_size, "%02d:%02d", (int)(m / 3600000), (int)((m / 60000) % 60));
}

void gt_times(int64_t start_ms, uint32_t duration_ms, char *out, size_t out_size)
{
    char from[8], to[8];
    gt_hhmm(start_ms, from, sizeof from);
    gt_hhmm(start_ms + (int64_t)duration_ms, to, sizeof to);
    snprintf(out, out_size, "%s " EN_DASH " %s", from, to);
}

void gt_episode(const ctw_episode *ep, char *out, size_t out_size)
{
    if (ep->title != NULL && ep->title[0] != '\0') {
        snprintf(out, out_size, "S%02uE%02u " MIDDOT " %s", (unsigned)ep->season, (unsigned)ep->episode, ep->title);
    } else {
        snprintf(out, out_size, "S%02uE%02u", (unsigned)ep->season, (unsigned)ep->episode);
    }
}

static void next_name(const ctw_airing *next, char *out, size_t out_size)
{
    if (next == NULL) {
        snprintf(out, out_size, "Up next");
    } else if (next->kind == CTW_AIR_AD) {
        snprintf(out, out_size, "Up next: Commercial break");
    } else if (next->kind == CTW_AIR_EPISODE && next->show != NULL) {
        snprintf(out, out_size, "Up next: %s", next->show->name);
    } else {
        snprintf(out, out_size, "Up next");
    }
}

void gt_line(const ctw_airing *air, const ctw_airing *next, char *out, size_t out_size)
{
    if (out_size == 0) return;
    out[0] = '\0';
    if (air == NULL) return;
    if (air->kind == CTW_AIR_AD) {
        snprintf(out, out_size, "Commercial break");
    } else if (air->kind == CTW_AIR_UPNEXT) {
        next_name(next, out, out_size);
    } else if (air->show != NULL && air->episode != NULL) {
        const char *title = air->episode->title != NULL ? air->episode->title : "";
        snprintf(out, out_size, "%s " MIDDOT " S%02uE%02u%s%s", air->show->name, (unsigned)air->episode->season,
                 (unsigned)air->episode->episode, title[0] != '\0' ? " " : "", title);
    }
}

float gt_progress(const ctw_airing *air, uint32_t offset_ms)
{
    if (air == NULL || air->duration_ms == 0) return 0.0f;
    if (offset_ms >= air->duration_ms) return 1.0f;
    return (float)offset_ms / (float)air->duration_ms;
}

const ctw_show *gt_line_show(const ctw_airing *air, const ctw_airing *next)
{
    if (air == NULL) return NULL;
    if (air->kind == CTW_AIR_EPISODE) return air->show;
    if (air->kind == CTW_AIR_UPNEXT && next != NULL && next->kind == CTW_AIR_EPISODE) return next->show;
    return NULL;
}

void gt_icon_path(const char *root, const ctw_channel *ch, const ctw_show *show, char *out, size_t out_size)
{
    const char *rel = show != NULL && show->icon != NULL && show->icon[0] != '\0' ? show->icon : ch->icon;
    snprintf(out, out_size, "%s/%s", root, rel != NULL ? rel : "");
}
