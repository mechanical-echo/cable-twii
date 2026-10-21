#include "audio.h"

#include <string.h>
#include <asndlib.h>
#include <ogc/lwp_watchdog.h>

#define AUDIO_VOICE 15
#define AUDIO_CHUNK 4096
#define AUDIO_CHUNK_BYTES (AUDIO_CHUNK * 4)
#define AUDIO_BUFS 2
#define AUDIO_RING (AUDIO_CHUNK * 16)

static u32 ring[AUDIO_RING] __attribute__((aligned(32)));
static u32 bufs[AUDIO_BUFS][AUDIO_CHUNK] __attribute__((aligned(32)));
static int buf_idx;
static volatile u32 r_pos, w_pos, consumed, underruns;
static volatile int running, need_base, eof;
static volatile double base_time;
static volatile u64 cb_tick;
static int volume = 255;

static u32 ring_fill(void)
{
    return w_pos - r_pos;
}

unsigned int au_ring_free(void)
{
    return AUDIO_RING - ring_fill();
}

unsigned int au_ring_fill(void)
{
    return ring_fill();
}

static void audio_cb(s32 voice)
{
    u32 *buf = bufs[buf_idx];
    buf_idx = (buf_idx + 1) % AUDIO_BUFS;
    u32 n = 0;
    if (running) {
        u32 avail = w_pos - r_pos;
        n = avail < AUDIO_CHUNK ? avail : AUDIO_CHUNK;
        u32 r = r_pos;
        for (u32 i = 0; i < n; i++) {
            buf[i] = ring[(r + i) & (AUDIO_RING - 1)];
        }
        r_pos = r + n;
        consumed += n;
        if (n < AUDIO_CHUNK && !eof) underruns++;
        cb_tick = gettime();
    }
    if (n < AUDIO_CHUNK) memset(buf + n, 0, (AUDIO_CHUNK - n) * 4);
    DCFlushRange(buf, AUDIO_CHUNK_BYTES);
    ASND_AddVoice(voice, buf, AUDIO_CHUNK_BYTES);
}

void au_init(void)
{
    au_reset();
}

void au_reset(void)
{
    u32 lvl = IRQ_Disable();
    running = 0;
    r_pos = 0;
    w_pos = 0;
    consumed = 0;
    underruns = 0;
    need_base = 1;
    eof = 0;
    IRQ_Restore(lvl);
}

void au_push(const float *interleaved, unsigned int count, double pts)
{
    if (au_ring_free() < count) return;
    if (need_base) {
        base_time = pts;
        need_base = 0;
    }
    u32 w = w_pos;
    for (unsigned int i = 0; i < count; i++) {
        float fl = interleaved[i * 2] * 32767.0f;
        float fr = interleaved[i * 2 + 1] * 32767.0f;
        int l = fl > 32767.0f ? 32767 : (fl < -32768.0f ? -32768 : (int)fl);
        int r = fr > 32767.0f ? 32767 : (fr < -32768.0f ? -32768 : (int)fr);
        ring[(w + i) & (AUDIO_RING - 1)] = ((u32)(u16)l << 16) | (u16)r;
    }
    w_pos = w + count;
}

void au_go(void)
{
    u32 lvl = IRQ_Disable();
    cb_tick = gettime();
    running = 1;
    IRQ_Restore(lvl);
    ASND_SetVoice(AUDIO_VOICE, VOICE_STEREO_16BIT, 48000, 0, bufs[0], AUDIO_CHUNK_BYTES, volume, volume, audio_cb);
}

double au_clock(int sample_rate)
{
    u32 lvl = IRQ_Disable();
    double b = base_time;
    u32 cons = consumed;
    u64 tick = cb_tick;
    int run = running;
    int need = need_base;
    IRQ_Restore(lvl);
    if (need) return -1.0;
    double t = b + ((double)cons - 2.0 * AUDIO_CHUNK) / sample_rate;
    if (run) {
        double e = (double)ticks_to_microsecs(diff_ticks(tick, gettime())) / 1000000.0;
        double lim = (double)AUDIO_CHUNK / sample_rate;
        t += e < lim ? e : lim;
    }
    return t;
}

void au_set_volume(int volume_0_100)
{
    int v = volume_0_100;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    volume = v * 255 / 100;
    ASND_ChangeVolumeVoice(AUDIO_VOICE, volume, volume);
}
