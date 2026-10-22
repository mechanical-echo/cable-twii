#include "audio.h"

#include <string.h>
#include <asndlib.h>
#include <ogc/lwp_watchdog.h>

#include "audiobuf.h"

#define AUDIO_VOICE 15
#define AUDIO_CHUNK 4096
#define AUDIO_CHUNK_BYTES (AUDIO_CHUNK * 4)
#define AUDIO_RING (AUDIO_CHUNK * 16)

static u32 ring[AUDIO_RING] __attribute__((aligned(32)));
static u32 bufs[AB_BUFS][AUDIO_CHUNK] __attribute__((aligned(32)));
static ab_state ab;
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

static void fill_buf(u32 *buf)
{
    if (running) {
        u32 r = r_pos;
        u32 n = ab_drain(buf, AUDIO_CHUNK, ring, AUDIO_RING, &r, w_pos);
        r_pos = r;
        consumed += n;
        if (n < AUDIO_CHUNK && !eof) underruns++;
        cb_tick = gettime();
    } else {
        memset(buf, 0, AUDIO_CHUNK_BYTES);
    }
    DCFlushRange(buf, AUDIO_CHUNK_BYTES);
}

static void audio_cb(s32 voice)
{
    u32 *buf = bufs[ab_take(&ab)];
    fill_buf(buf);
    ASND_AddVoice(voice, buf, AUDIO_CHUNK_BYTES);
}

void au_init(void)
{
    au_reset();
}

void au_reset(void)
{
    ASND_StopVoice(AUDIO_VOICE);
    u32 lvl = IRQ_Disable();
    running = 0;
    r_pos = 0;
    w_pos = 0;
    consumed = 0;
    underruns = 0;
    need_base = 1;
    eof = 0;
    ab_start(&ab);
    memset(bufs, 0, sizeof bufs);
    DCFlushRange(bufs, sizeof bufs);
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
    u32 *first = bufs[ab_start(&ab)];
    cb_tick = gettime();
    running = 1;
    fill_buf(first);
    IRQ_Restore(lvl);
    ASND_SetVoice(AUDIO_VOICE, VOICE_STEREO_16BIT, 48000, 0, first, AUDIO_CHUNK_BYTES, volume, volume, audio_cb);
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
