#include "player.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <grrlib.h>
#include <ogc/lwp.h>
#include <ogc/mutex.h>
#include <ogc/lwp_watchdog.h>

#define PLM_BUFFER_DEFAULT_SIZE (256 * 1024)
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "yuvgx.h"
#include "audio.h"
#include "playback.h"

#define SLOT_FREE 0
#define SLOT_READY 1
#define SLOT_SHOWN 2
#define DECODE_STACK (128 * 1024)
#define DECODE_PRIO 48
#define AUDIO_LEAD_SAMPLES 12000
#define LATE_FRAME_LIMIT 2

typedef struct {
    volatile int state;
    double time;
    int w, h;
} slot_meta;

static plm_t *plm;
static FILE *fh;
static char cur_path[320];
static int has_audio_s;
static int samplerate_s;
static double duration_s;
static int vid_w, vid_h;

static slot_meta slots[YG_SLOTS];
static mutex_t q_lock;
static int q_r, q_w, shown;

static lwp_t dec_thread;
static volatile int dec_quit;
static volatile int seek_req;
static volatile double seek_target_s;
static volatile int in_seek;
static volatile int video_eof, audio_eof;
static volatile double wall_base;
static volatile u64 wall_tick;
static volatile int wall_running;
static volatile pl_state status = PL_IDLE;
static int inited;
static double last_shown_time;

static double av_clock(void)
{
    if (!has_audio_s) {
        if (!wall_running) return wall_base;
        return wall_base + (double)ticks_to_microsecs(diff_ticks(wall_tick, gettime())) / 1000000.0;
    }
    double t = au_clock(samplerate_s);
    return t < 0.0 ? 0.0 : t;
}

static void push_frame(plm_frame_t *f)
{
    slot_meta *s = &slots[q_w];
    if (s->state != SLOT_FREE) return;
    yg_upload(q_w, f->y.data, f->cb.data, f->cr.data, f->width, f->height);
    s->time = f->time;
    s->w = (int)f->width;
    s->h = (int)f->height;
    LWP_MutexLock(q_lock);
    s->state = SLOT_READY;
    q_w = (q_w + 1) % YG_SLOTS;
    LWP_MutexUnlock(q_lock);
}

static void on_video(plm_t *p, plm_frame_t *f, void *user)
{
    (void)p;
    (void)user;
    if (in_seek) push_frame(f);
}

static void on_audio(plm_t *p, plm_samples_t *s, void *user)
{
    (void)p;
    (void)user;
    au_push(s->interleaved, s->count, s->time);
}

static void flush_queue(void)
{
    LWP_MutexLock(q_lock);
    for (int i = 0; i < YG_SLOTS; i++) {
        if (slots[i].state == SLOT_READY) slots[i].state = SLOT_FREE;
    }
    if (shown >= 0) q_w = (shown + 1) % YG_SLOTS;
    q_r = q_w;
    LWP_MutexUnlock(q_lock);
}

static void on_stream_end(void)
{
    if (status == PL_ERROR) return;
    struct stat st;
    bool path_ok = stat(cur_path, &st) == 0;
    if (pb_classify_end(ferror(fh) != 0, path_ok) == PB_END_ERROR) status = PL_ERROR;
}

static void prefill_audio(void)
{
    while (has_audio_s && !audio_eof && au_ring_fill() < AUDIO_LEAD_SAMPLES) {
        plm_samples_t *s = plm_decode_audio(plm);
        if (s == NULL) {
            audio_eof = 1;
            on_stream_end();
            break;
        }
        au_push(s->interleaved, s->count, s->time);
    }
}

static void do_seek(void)
{
    double target = seek_target_s;
    seek_req = 0;
    au_reset();
    wall_running = 0;
    flush_queue();
    in_seek = 1;
    plm_seek(plm, target, 0);
    in_seek = 0;
    video_eof = 0;
    audio_eof = 0;
    prefill_audio();
    if (!has_audio_s) {
        wall_base = target;
        wall_tick = gettime();
        wall_running = 1;
    } else {
        au_go();
    }
    if (ferror(fh)) status = PL_ERROR;
    if (status != PL_ERROR) status = PL_PLAYING;
}

static void *decode_thread(void *arg)
{
    (void)arg;
    while (!dec_quit) {
        if (status == PL_ERROR) {
            usleep(10000);
            continue;
        }
        if (seek_req) {
            do_seek();
            continue;
        }
        int did = 0;
        if (has_audio_s && !audio_eof && au_ring_fill() < AUDIO_LEAD_SAMPLES) {
            plm_samples_t *s = plm_decode_audio(plm);
            if (s != NULL) {
                au_push(s->interleaved, s->count, s->time);
            } else {
                audio_eof = 1;
                on_stream_end();
            }
            did = 1;
        } else if (!video_eof && slots[q_w].state == SLOT_FREE) {
            plm_frame_t *f = plm_decode_video(plm);
            if (f != NULL) {
                push_frame(f);
            } else {
                video_eof = 1;
                on_stream_end();
            }
            did = 1;
        }
        if (!did) usleep(2000);
    }
    return NULL;
}

int pl_open(const char *path, uint32_t offset_ms)
{
    pl_close();

    snprintf(cur_path, sizeof cur_path, "%s", path);
    fh = fopen(path, "rb");
    if (fh == NULL) {
        status = PL_ERROR;
        return -1;
    }
    plm = plm_create_with_file(fh, 1);
    if (plm == NULL) {
        fclose(fh);
        fh = NULL;
        status = PL_ERROR;
        return -1;
    }
    if (!plm_has_headers(plm)) {
        plm_destroy(plm);
        plm = NULL;
        fh = NULL;
        status = PL_ERROR;
        return -1;
    }

    vid_w = plm_get_width(plm);
    vid_h = plm_get_height(plm);
    has_audio_s = plm_get_audio_enabled(plm) && plm_get_samplerate(plm) > 0;
    samplerate_s = plm_get_samplerate(plm);
    duration_s = plm_get_duration(plm);

    if (yg_init(vid_w, vid_h) != 0) {
        plm_destroy(plm);
        plm = NULL;
        fh = NULL;
        status = PL_ERROR;
        return -1;
    }

    for (int i = 0; i < YG_SLOTS; i++) {
        slots[i].state = SLOT_FREE;
    }
    q_r = q_w = 0;
    shown = -1;
    last_shown_time = -1.0;

    if (!inited) {
        LWP_MutexInit(&q_lock, false);
        inited = 1;
    }

    plm_set_video_decode_callback(plm, on_video, NULL);
    plm_set_audio_decode_callback(plm, on_audio, NULL);
    au_init();

    dec_quit = 0;
    status = PL_SEEKING;
    LWP_CreateThread(&dec_thread, decode_thread, NULL, NULL, DECODE_STACK, DECODE_PRIO);

    seek_target_s = (double)offset_ms / 1000.0;
    seek_req = 1;
    return 0;
}

void pl_close(void)
{
    if (plm == NULL) {
        status = PL_IDLE;
        return;
    }
    dec_quit = 1;
    LWP_JoinThread(dec_thread, NULL);
    yg_free();
    plm_destroy(plm);
    plm = NULL;
    fh = NULL;
    au_reset();
    status = PL_IDLE;
}

pl_state pl_status(void)
{
    return status;
}

void pl_set_volume(int volume_0_100)
{
    au_set_volume(volume_0_100);
}

uint32_t pl_position_ms(void)
{
    return (uint32_t)(av_clock() * 1000.0);
}

int pl_video_size(int *w, int *h)
{
    if (plm == NULL) return -1;
    if (w != NULL) *w = vid_w;
    if (h != NULL) *h = vid_h;
    return 0;
}

void pl_draw(float x, float y, float w, float h, float alpha)
{
    if (plm == NULL || status == PL_IDLE || status == PL_ENDED || status == PL_ERROR) {
        GRRLIB_Rectangle(x, y, w, h, RGBA(0, 0, 0, 0xFF), 1);
        return;
    }

    double now = av_clock();
    int advanced = 0;
    LWP_MutexLock(q_lock);
    while (q_r != q_w) {
        int idx = q_r;
        if (slots[idx].state != SLOT_READY) break;
        double frame_dur = samplerate_s > 0 ? 1.0 / 30.0 : 1.0 / 30.0;
        if (slots[idx].time > now + 0.001 && advanced) break;
        if (shown >= 0 && slots[shown].state == SLOT_SHOWN) slots[shown].state = SLOT_FREE;
        shown = idx;
        slots[idx].state = SLOT_SHOWN;
        q_r = (q_r + 1) % YG_SLOTS;
        advanced = 1;
        last_shown_time = slots[idx].time;
        if (slots[idx].time > now - LATE_FRAME_LIMIT * frame_dur) break;
    }
    int draw_slot = shown;
    int fw = draw_slot >= 0 ? slots[draw_slot].w : 0;
    int fh = draw_slot >= 0 ? slots[draw_slot].h : 0;
    LWP_MutexUnlock(q_lock);

    if (draw_slot < 0) {
        GRRLIB_Rectangle(x, y, w, h, RGBA(0, 0, 0, 0xFF), 1);
        return;
    }

    double target_aspect = (double)w / (double)h;
    double frame_aspect = (double)fw / (double)fh;
    float qw = w, qh = h;
    if (frame_aspect >= target_aspect) {
        qh = (float)(h * target_aspect / frame_aspect);
    } else {
        qw = (float)(w * frame_aspect / target_aspect);
    }
    float dx = x + (w - qw) / 2.0f;
    float dy = y + (h - qh) / 2.0f;
    if (qw < w || qh < h) GRRLIB_Rectangle(x, y, w, h, RGBA(0, 0, 0, 0xFF), 1);
    yg_draw(draw_slot, fw, fh, dx, dy, qw, qh);
    if (alpha < 1.0f) {
        uint8_t fade = (uint8_t)((1.0f - (alpha < 0.0f ? 0.0f : alpha)) * 255.0f);
        GRRLIB_Rectangle(dx, dy, qw, qh, RGBA(0, 0, 0, fade), 1);
    }

    if (status != PL_ERROR && video_eof && has_audio_s == 0 && !wall_running) status = PL_ENDED;
    if (status != PL_ERROR && video_eof && audio_eof) status = PL_ENDED;
    (void)duration_s;
}
