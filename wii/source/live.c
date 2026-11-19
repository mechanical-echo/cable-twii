#include "live.h"

#include <stdio.h>

#include "clock.h"
#include "playback.h"
#include "storage.h"
#include "media/player.h"

static bool tuned;
static int tuned_channel;
static int64_t current_start_ms = -1;
static bool end_probed;
static bool playing;
static uint32_t playing_since_ms;
static bool streaming;
static bool stalled;
static bool lost;
static bool probed;
static uint32_t probe_ms;

static void play_airing(app *a, int channel, const ctw_airing *air, uint32_t offset_ms)
{
    end_probed = false;
    playing = false;
    lost = lost || stalled;
    stalled = false;
    streaming = air != NULL && air->kind != CTW_AIR_UPNEXT;
    if (!streaming || lost) {
        pl_close();
        current_start_ms = air != NULL ? tn_airing_start(&a->tuner, channel, air) : -1;
        pl_set_volume(a->volume);
        return;
    }
    char path[320];
    const char *rel = air->kind == CTW_AIR_AD ? air->ad->path : air->episode->path;
    snprintf(path, sizeof path, "%s/%s", a->root, rel);
    pl_open(path, offset_ms);
    pl_set_volume(a->volume);
    current_start_ms = tn_airing_start(&a->tuner, channel, air);
}

void live_tune(app *a, int channel)
{
    a->current_channel = channel;
    tuned = true;
    tuned_channel = channel;
    clk_now(&a->today, &a->ms_of_day);
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, channel, a->today, a->ms_of_day, &offset);
    play_airing(a, channel, air, offset);
}

void live_stop(void)
{
    pl_close();
    tuned = false;
    playing = false;
    streaming = false;
    stalled = false;
    lost = false;
    probed = false;
    end_probed = false;
    current_start_ms = -1;
}

bool live_on(int channel)
{
    return tuned && tuned_channel == channel;
}

static bool drive_lost(app *a, const ctw_airing *air, uint32_t offset)
{
    pl_state st = pl_status();
    uint32_t remaining = air != NULL ? tn_ms_remaining(air, offset) : 0;
    if (!pb_needs_probe(st, remaining)) return false;
    if (st == PL_ENDED && end_probed) return false;
    end_probed = true;
    if (st == PL_ERROR) pl_close();
    if (pb_on_status(st, remaining, st_drive_present(&a->drive)) != PB_DISCONNECTED) return false;
    app_fail(a, "The drive was disconnected", "Plug it back in and start cable-twii again.");
    return true;
}

static void server_update(app *a, const ctw_airing *air, uint32_t offset)
{
    pl_state st = pl_status();
    uint32_t played = playing ? a->now_ms - playing_since_ms : 0;
    uint32_t waiting = pb_play_waiting(st, pl_waiting_ms(), played);
    if (pb_lost(st, waiting)) {
        lost = true;
        stalled = false;
    }
    if (lost || (st != PL_PLAYING && st != PL_SEEKING)) {
        stalled = false;
        return;
    }
    pb_stall act = pb_stall_action(st, waiting, stalled, pl_position_ms(), offset);
    stalled = act == PB_STALL_NOTE;
    if (act == PB_STALL_REJOIN) play_airing(a, tuned_channel, air, offset);
}

bool live_update(app *a)
{
    clk_now(&a->today, &a->ms_of_day);
    if (!tuned) return true;
    if (!playing && pl_status() == PL_PLAYING) {
        playing = true;
        playing_since_ms = a->now_ms;
        stalled = false;
    }
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, tuned_channel, a->today, a->ms_of_day, &offset);
    if (air != NULL && tn_airing_start(&a->tuner, tuned_channel, air) != current_start_ms) {
        play_airing(a, tuned_channel, air, offset);
        return true;
    }
    if (!a->from_server) return !drive_lost(a, air, offset);
    server_update(a, air, offset);
    return true;
}

bool live_probe_due(const app *a)
{
    return tuned && lost && pb_probe_due(probed, a->now_ms - probe_ms);
}

void live_probe_sent(const app *a)
{
    probed = true;
    probe_ms = a->now_ms;
}

void live_probe_done(app *a, bool answered)
{
    if (!answered || !tuned || !lost) return;
    lost = false;
    clk_now(&a->today, &a->ms_of_day);
    uint32_t offset = 0;
    const ctw_airing *air = tn_current(&a->tuner, tuned_channel, a->today, a->ms_of_day, &offset);
    play_airing(a, tuned_channel, air, offset);
}

bool live_weak(void)
{
    return tuned && streaming && (lost || stalled);
}

float live_fade_in(const app *a, uint32_t fade_ms)
{
    if (!playing) return 0.0f;
    uint32_t elapsed = a->now_ms - playing_since_ms;
    if (fade_ms == 0 || elapsed >= fade_ms) return 1.0f;
    return (float)elapsed / (float)fade_ms;
}
