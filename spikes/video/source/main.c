#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include <ogc/lwp_watchdog.h>
#include <asndlib.h>
#include <dirent.h>
#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define PLM_BUFFER_DEFAULT_SIZE (256 * 1024)
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#ifndef SEEK_TARGET
#define SEEK_TARGET 300.0
#endif

#define SPIKE_DIR "/cable-twii-spike"
#define DEFAULT_FILE "test.mpg"
#define LOG_NAME "spike-log.txt"
#define FIFO_SIZE (256 * 1024)
#define SLOT_COUNT 4
#define AUDIO_CHUNK 1024
#define AUDIO_CHUNK_BYTES (AUDIO_CHUNK * 4)
#define AUDIO_BUFS 3
#define AUDIO_RING 32768
#define AUDIO_LEAD 12000
#define MAX_FILES 64
#define LOG_INTERVAL 10.0
#define DECODE_STACK (128 * 1024)
#define DECODE_PRIO 48
#define MAIN_PRIO 80
#define DUE_SLACK 0.008
#define CHAR_W 16
#define CHAR_H 32

enum { MODE_TEV = 0, MODE_CPU = 1 };
enum { SLOT_FREE = 0, SLOT_READY = 1, SLOT_SHOWN = 2 };
enum {
	BTN_UP = 1 << 0,
	BTN_DOWN = 1 << 1,
	BTN_A = 1 << 2,
	BTN_B = 1 << 3,
	BTN_1 = 1 << 4,
	BTN_2 = 1 << 5,
	BTN_PLUS = 1 << 6,
	BTN_MINUS = 1 << 7,
	BTN_HOME = 1 << 8
};

typedef struct {
	volatile int state;
	int mode;
	int gen;
	double time;
	int lw, lh, fw, fh;
	u8 *y, *cb, *cr;
	u16 *rgb;
	GXTexObj ty, tcb, tcr, trgb;
} slot_t;

typedef struct {
	char path[256];
	char label[160];
} entry_t;

extern u8 console_font_8x16[];

static GXRModeObj *rmode;
static void *xfb[2];
static int fbi;
static int widescreen;
static int refresh_hz;
static u8 font_tex[128 * 256] __attribute__((aligned(32)));
static GXTexObj font_obj;
static volatile int reset_pressed;

static FILE *log_fp;
static mutex_t log_lock;

static entry_t entries[MAX_FILES];
static int entry_count;
static char last_result[6][48];
static int last_result_lines;

static plm_t *plm;
static int has_audio;
static int samplerate;
static double duration;
static int vid_w, vid_h;
static double vid_par;
static slot_t slots[SLOT_COUNT];
static mutex_t q_lock;
static int q_r, q_w, shown;
static volatile int cur_mode;
static volatile int dec_quit;
static volatile int seek_req;
static volatile double seek_target_req;
static volatile int seek_exact;
static volatile int seek_gen;
static volatile int in_seek;
static volatile double seek_ms;
static volatile double seek_pts;
static volatile int seek_ok;
static volatile int seek_done_gen;
static volatile int video_eof, audio_eof;
static volatile int stats_reset_req;
static volatile double dec_sum, dec_max, cnv_sum, cnv_max;
static volatile u32 dec_count, cnv_count;
static volatile u64 wall_tick;
static volatile double wall_base;
static volatile int wall_running;

static u32 audio_ring[AUDIO_RING] __attribute__((aligned(32)));
static u32 audio_bufs[AUDIO_BUFS][AUDIO_CHUNK] __attribute__((aligned(32)));
static int audio_buf_idx;
static volatile u32 a_r, a_w, a_consumed, a_underruns;
static volatile int a_running, a_need_base, a_eof;
static volatile double a_base;
static volatile u64 a_cb_tick;

static void log_line(const char *fmt, ...)
{
	char buf[384];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	printf("%s\n", buf);
	LWP_MutexLock(log_lock);
	if (log_fp) {
		fprintf(log_fp, "%s\n", buf);
		fflush(log_fp);
	}
	LWP_MutexUnlock(log_lock);
}

static double us_between(u64 a, u64 b)
{
	return (double)ticks_to_microsecs(diff_ticks(a, b));
}

static void reset_cb(u32 irq, void *ctx)
{
	reset_pressed = 1;
}

static void power_cb(void)
{
	reset_pressed = 1;
}

static void tile_i8(u8 *dst, const u8 *src, int w, int h, int stride)
{
	u32 *d = (u32 *)dst;
	for (int y = 0; y < h; y += 4) {
		const u8 *r0 = src + y * stride;
		const u8 *r1 = r0 + stride;
		const u8 *r2 = r1 + stride;
		const u8 *r3 = r2 + stride;
		for (int x = 0; x < w; x += 8) {
			const u32 *a = (const u32 *)(r0 + x);
			const u32 *b = (const u32 *)(r1 + x);
			const u32 *c = (const u32 *)(r2 + x);
			const u32 *e = (const u32 *)(r3 + x);
			d[0] = a[0];
			d[1] = a[1];
			d[2] = b[0];
			d[3] = b[1];
			d[4] = c[0];
			d[5] = c[1];
			d[6] = e[0];
			d[7] = e[1];
			d += 8;
		}
	}
}

static inline int clamp8(int v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static inline u16 pack565(int l, int r, int g, int b)
{
	int R = clamp8(l + r);
	int G = clamp8(l - g);
	int B = clamp8(l + b);
	return (u16)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
}

static void convert_rgb565(u16 *dst, const plm_frame_t *f)
{
	int w = f->y.width;
	int h = f->y.height;
	int cw = f->cb.width;
	int tiles_x = w >> 2;
	for (int y = 0; y < h; y += 2) {
		const u8 *y0 = f->y.data + y * w;
		const u8 *y1 = y0 + w;
		const u8 *cb = f->cb.data + (y >> 1) * cw;
		const u8 *cr = f->cr.data + (y >> 1) * cw;
		u16 *row = dst + (((y >> 2) * tiles_x) << 4) + ((y & 3) << 2);
		for (int x = 0; x < w; x += 2) {
			int u = cb[x >> 1] - 128;
			int v = cr[x >> 1] - 128;
			int r = (v * 104597) >> 16;
			int g = (u * 25674 + v * 53278) >> 16;
			int b = (u * 132201) >> 16;
			u16 *t = row + ((x >> 2) << 4) + (x & 3);
			t[0] = pack565(((y0[x] - 16) * 76309) >> 16, r, g, b);
			t[1] = pack565(((y0[x + 1] - 16) * 76309) >> 16, r, g, b);
			t[4] = pack565(((y1[x] - 16) * 76309) >> 16, r, g, b);
			t[5] = pack565(((y1[x + 1] - 16) * 76309) >> 16, r, g, b);
		}
	}
}

static void build_font(void)
{
	static u8 linear[128 * 256];
	for (int c = 0; c < 256; c++) {
		int ox = (c & 15) * 8;
		int oy = (c >> 4) * 16;
		for (int r = 0; r < 16; r++) {
			u8 bits = console_font_8x16[c * 16 + r];
			for (int i = 0; i < 8; i++) {
				linear[(oy + r) * 128 + ox + i] = (bits & (0x80 >> i)) ? 255 : 0;
			}
		}
	}
	tile_i8(font_tex, linear, 128, 256, 128);
	DCFlushRange(font_tex, sizeof(font_tex));
	GX_InitTexObj(&font_obj, font_tex, 128, 256, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjFilterMode(&font_obj, GX_NEAR, GX_NEAR);
}

static void video_init(void)
{
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	widescreen = CONF_GetAspectRatio() == CONF_ASPECT_16_9;
	if (widescreen) {
		rmode->viWidth = 678;
		rmode->viXOrigin = (VI_MAX_WIDTH_NTSC - 678) / 2;
	}
	VIDEO_Configure(rmode);
	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_ClearFrameBuffer(rmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(rmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	fbi = 1;
	refresh_hz = VIDEO_GetCurrentTvMode() == VI_PAL ? 50 : 60;

	void *fifo = memalign(32, FIFO_SIZE);
	memset(fifo, 0, FIFO_SIZE);
	GX_Init(fifo, FIFO_SIZE);
	GXColor bg = {0, 0, 0, 255};
	GX_SetCopyClear(bg, 0x00ffffff);
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
	u32 xfb_h = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth, xfb_h);
	GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering, (rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);
	GX_SetPixelFmt(rmode->aa ? GX_PF_RGB565_Z16 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_CopyDisp(xfb[0], GX_TRUE);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetNumChans(1);
	GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GX_SetColorUpdate(GX_TRUE);
	GX_SetAlphaUpdate(GX_FALSE);

	Mtx44 proj;
	guOrtho(proj, 0, 480, 0, 640, 0, 300);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
	Mtx mv;
	guMtxIdentity(mv);
	guMtxTransApply(mv, mv, 0.0f, 0.0f, -5.0f);
	GX_LoadPosMtxImm(mv, GX_PNMTX0);

	build_font();
}

static void quad(float x0, float y0, float x1, float y1, float s0, float t0, float s1, float t1, GXColor c)
{
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2f32(x0, y0);
	GX_Color4u8(c.r, c.g, c.b, c.a);
	GX_TexCoord2f32(s0, t0);
	GX_Position2f32(x1, y0);
	GX_Color4u8(c.r, c.g, c.b, c.a);
	GX_TexCoord2f32(s1, t0);
	GX_Position2f32(x1, y1);
	GX_Color4u8(c.r, c.g, c.b, c.a);
	GX_TexCoord2f32(s1, t1);
	GX_Position2f32(x0, y1);
	GX_Color4u8(c.r, c.g, c.b, c.a);
	GX_TexCoord2f32(s0, t1);
	GX_End();
}

static void tev_single(u8 texmap, u8 op)
{
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevOrder(GX_TEVSTAGE0, texmap == GX_TEXMAP_NULL ? GX_TEXCOORDNULL : GX_TEXCOORD0, texmap, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, op);
}

static void tev_yuv_stage(u8 stage, u8 texcoord, u32 texmap, u8 ksel, u8 d, u8 op, u8 scale, u8 clamp)
{
	GX_SetTevDirect(stage);
	GX_SetTevOrder(stage, texcoord, texmap, GX_COLORNULL);
	GX_SetTevKColorSel(stage, ksel);
	GX_SetTevKAlphaSel(stage, GX_TEV_KASEL_1);
	GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, d);
	GX_SetTevColorOp(stage, op, GX_TB_ZERO, scale, clamp, GX_TEVPREV);
	GX_SetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

static void tev_yuv(void)
{
	GXColorS10 offset = {-112, 68, -137, 255};
	GX_SetTevColorS10(GX_TEVREG0, offset);
	GX_SetTevKColor(GX_KCOLOR0, (GXColor){204, 0, 0, 148});
	GX_SetTevKColor(GX_KCOLOR1, (GXColor){0, 0, 255, 0});
	GX_SetTevKColor(GX_KCOLOR2, (GXColor){0, 50, 0, 0});
	GX_SetTevKColor(GX_KCOLOR3, (GXColor){0, 104, 0, 0});
	GX_SetNumTexGens(2);
	GX_SetNumTevStages(5);
	tev_yuv_stage(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_TEV_KCSEL_K0_A, GX_CC_C0, GX_TEV_ADD, GX_CS_SCALE_1, GX_FALSE);
	tev_yuv_stage(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_TEV_KCSEL_K1, GX_CC_CPREV, GX_TEV_ADD, GX_CS_SCALE_1, GX_FALSE);
	tev_yuv_stage(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP1, GX_TEV_KCSEL_K2, GX_CC_CPREV, GX_TEV_SUB, GX_CS_SCALE_1, GX_FALSE);
	tev_yuv_stage(GX_TEVSTAGE3, GX_TEXCOORD1, GX_TEXMAP2, GX_TEV_KCSEL_K0, GX_CC_CPREV, GX_TEV_ADD, GX_CS_SCALE_1, GX_FALSE);
	tev_yuv_stage(GX_TEVSTAGE4, GX_TEXCOORD1, GX_TEXMAP2, GX_TEV_KCSEL_K3, GX_CC_CPREV, GX_TEV_SUB, GX_CS_SCALE_2, GX_TRUE);
}

static void draw_box(float x0, float y0, float x1, float y1, GXColor c)
{
	tev_single(GX_TEXMAP_NULL, GX_PASSCLR);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	quad(x0, y0, x1, y1, 0, 0, 0, 0, c);
}

static void draw_text(float x, float y, GXColor c, const char *s)
{
	GX_LoadTexObj(&font_obj, GX_TEXMAP0);
	tev_single(GX_TEXMAP0, GX_MODULATE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	for (; *s; s++, x += CHAR_W) {
		u8 ch = (u8)*s;
		float s0 = (ch & 15) * 8 / 128.0f;
		float t0 = (ch >> 4) * 16 / 256.0f;
		quad(x, y, x + CHAR_W, y + CHAR_H, s0, t0, s0 + 8 / 128.0f, t0 + 16 / 256.0f, c);
	}
}

static void draw_textf(float x, float y, GXColor c, const char *fmt, ...)
{
	char buf[96];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	draw_text(x, y, c, buf);
}

static void frame_begin(void)
{
	GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
	GX_InvVtxCache();
	GX_InvalidateTexAll();
}

static void frame_end(void)
{
	GX_CopyDisp(xfb[fbi], GX_TRUE);
	GX_DrawDone();
	VIDEO_SetNextFramebuffer(xfb[fbi]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	fbi ^= 1;
}

#ifdef AUTOTEST
static u64 auto_start;
static int auto_step;

static u32 auto_buttons(void)
{
	static const double at[4] = {6.0, 12.0, 18.0, 24.0};
	static const u32 btn[4] = {BTN_1, BTN_2, BTN_MINUS, BTN_HOME};
	if (!auto_start || auto_step >= 4) {
		return 0;
	}
	if (us_between(auto_start, gettime()) / 1000000.0 >= at[auto_step]) {
		return btn[auto_step++];
	}
	return 0;
}
#endif

static u32 read_buttons(void)
{
	WPAD_ScanPads();
	PAD_ScanPads();
	u32 w = WPAD_ButtonsDown(0);
	u32 p = PAD_ButtonsDown(0);
	u32 b = 0;
	if (w & (WPAD_BUTTON_UP | WPAD_BUTTON_LEFT) || p & PAD_BUTTON_UP) b |= BTN_UP;
	if (w & (WPAD_BUTTON_DOWN | WPAD_BUTTON_RIGHT) || p & PAD_BUTTON_DOWN) b |= BTN_DOWN;
	if (w & WPAD_BUTTON_A || p & PAD_BUTTON_A) b |= BTN_A;
	if (w & WPAD_BUTTON_B || p & PAD_BUTTON_B) b |= BTN_B;
	if (w & WPAD_BUTTON_1 || p & PAD_BUTTON_X) b |= BTN_1;
	if (w & WPAD_BUTTON_2 || p & PAD_BUTTON_Y) b |= BTN_2;
	if (w & WPAD_BUTTON_PLUS || p & PAD_TRIGGER_R) b |= BTN_PLUS;
	if (w & WPAD_BUTTON_MINUS || p & PAD_TRIGGER_L) b |= BTN_MINUS;
	if (w & WPAD_BUTTON_HOME || p & PAD_BUTTON_START || reset_pressed) b |= BTN_HOME;
#ifdef AUTOTEST
	b |= auto_buttons();
#endif
	return b;
}

static u32 ring_fill(void)
{
	return a_w - a_r;
}

static void audio_cb(s32 voice)
{
	u32 *buf = audio_bufs[audio_buf_idx];
	audio_buf_idx = (audio_buf_idx + 1) % AUDIO_BUFS;
	u32 n = 0;
	if (a_running) {
		u32 avail = a_w - a_r;
		n = avail < AUDIO_CHUNK ? avail : AUDIO_CHUNK;
		u32 r = a_r;
		for (u32 i = 0; i < n; i++) {
			buf[i] = audio_ring[(r + i) & (AUDIO_RING - 1)];
		}
		a_r = r + n;
		a_consumed += n;
		if (n < AUDIO_CHUNK && !a_eof) {
			a_underruns++;
		}
		a_cb_tick = gettime();
	}
	if (n < AUDIO_CHUNK) {
		memset(buf + n, 0, (AUDIO_CHUNK - n) * 4);
	}
	DCFlushRange(buf, AUDIO_CHUNK_BYTES);
	ASND_AddVoice(voice, buf, AUDIO_CHUNK_BYTES);
}

static void audio_reset(void)
{
	u32 lvl = IRQ_Disable();
	a_running = 0;
	a_r = 0;
	a_w = 0;
	a_consumed = 0;
	a_need_base = 1;
	a_eof = 0;
	IRQ_Restore(lvl);
}

static void audio_go(void)
{
	u32 lvl = IRQ_Disable();
	a_cb_tick = gettime();
	a_running = 1;
	IRQ_Restore(lvl);
}

static void push_audio(const plm_samples_t *s)
{
	if (AUDIO_RING - ring_fill() < s->count) {
		return;
	}
	if (a_need_base) {
		a_base = s->time;
		a_need_base = 0;
	}
	u32 w = a_w;
	for (unsigned int i = 0; i < s->count; i++) {
		float fl = s->interleaved[i * 2] * 32767.0f;
		float fr = s->interleaved[i * 2 + 1] * 32767.0f;
		int l = fl > 32767.0f ? 32767 : (fl < -32768.0f ? -32768 : (int)fl);
		int r = fr > 32767.0f ? 32767 : (fr < -32768.0f ? -32768 : (int)fr);
		audio_ring[(w + i) & (AUDIO_RING - 1)] = ((u32)(u16)l << 16) | (u16)r;
	}
	a_w = w + s->count;
}

static double av_clock(void)
{
	if (!has_audio) {
		if (!wall_running) {
			return wall_base;
		}
		return wall_base + us_between(wall_tick, gettime()) / 1000000.0;
	}
	u32 lvl = IRQ_Disable();
	double base = a_base;
	u32 cons = a_consumed;
	u64 tick = a_cb_tick;
	int run = a_running;
	int need = a_need_base;
	IRQ_Restore(lvl);
	if (need) {
		return -1.0;
	}
	double t = base + ((double)cons - 2.0 * AUDIO_CHUNK) / samplerate;
	if (run) {
		double e = us_between(tick, gettime()) / 1000000.0;
		double lim = (double)AUDIO_CHUNK / samplerate;
		t += e < lim ? e : lim;
	}
	return t;
}

static void flush_queue(void)
{
	LWP_MutexLock(q_lock);
	for (int i = 0; i < SLOT_COUNT; i++) {
		if (slots[i].state == SLOT_READY) {
			slots[i].state = SLOT_FREE;
		}
	}
	if (shown >= 0) {
		q_w = (shown + 1) % SLOT_COUNT;
	}
	q_r = q_w;
	LWP_MutexUnlock(q_lock);
}

static void push_frame(const plm_frame_t *f)
{
	slot_t *s = &slots[q_w];
	if (s->state != SLOT_FREE) {
		return;
	}
	int mode = cur_mode;
	u64 t0 = gettime();
	if (mode == MODE_TEV) {
		tile_i8(s->y, f->y.data, f->y.width, f->y.height, f->y.width);
		tile_i8(s->cb, f->cb.data, f->cb.width, f->cb.height, f->cb.width);
		tile_i8(s->cr, f->cr.data, f->cr.width, f->cr.height, f->cr.width);
		DCFlushRange(s->y, s->lw * s->lh);
		DCFlushRange(s->cb, s->lw * s->lh / 4);
		DCFlushRange(s->cr, s->lw * s->lh / 4);
	} else {
		convert_rgb565(s->rgb, f);
		DCFlushRange(s->rgb, s->lw * s->lh * 2);
	}
	double ms = us_between(t0, gettime()) / 1000.0;
	if (!in_seek) {
		cnv_sum += ms;
		cnv_count++;
		if (ms > cnv_max) {
			cnv_max = ms;
		}
	}
	s->mode = mode;
	s->time = f->time;
	s->fw = f->width;
	s->fh = f->height;
	s->gen = seek_gen;
	LWP_MutexLock(q_lock);
	s->state = SLOT_READY;
	q_w = (q_w + 1) % SLOT_COUNT;
	LWP_MutexUnlock(q_lock);
}

static void on_video(plm_t *p, plm_frame_t *f, void *user)
{
	if (in_seek) {
		seek_pts = f->time;
		push_frame(f);
	}
}

static void on_audio(plm_t *p, plm_samples_t *s, void *user)
{
	push_audio(s);
}

static void prefill_audio(void)
{
	while (has_audio && !audio_eof && ring_fill() < AUDIO_LEAD) {
		plm_samples_t *s = plm_decode_audio(plm);
		if (!s) {
			audio_eof = 1;
			a_eof = 1;
			break;
		}
		push_audio(s);
	}
}

static void do_seek(void)
{
	double target = seek_target_req;
	int exact = seek_exact;
	seek_req = 0;
	audio_reset();
	wall_running = 0;
	flush_queue();
	seek_gen++;
	seek_pts = -1.0;
	in_seek = 1;
	u64 t0 = gettime();
	int ok = plm_seek(plm, target, exact);
	u64 t1 = gettime();
	in_seek = 0;
	video_eof = 0;
	audio_eof = 0;
	seek_ms = us_between(t0, t1) / 1000.0;
	seek_ok = ok;
	prefill_audio();
	if (!has_audio) {
		wall_base = seek_pts >= 0 ? seek_pts : target;
		wall_tick = gettime();
		wall_running = 1;
	} else {
		audio_go();
	}
	seek_done_gen = seek_gen;
}

static void *decode_thread(void *arg)
{
	while (!dec_quit) {
		if (stats_reset_req) {
			dec_sum = dec_max = cnv_sum = cnv_max = 0;
			dec_count = cnv_count = 0;
			stats_reset_req = 0;
		}
		if (seek_req) {
			do_seek();
			continue;
		}
		int did = 0;
		if (has_audio && !audio_eof && ring_fill() < AUDIO_LEAD) {
			plm_samples_t *s = plm_decode_audio(plm);
			if (s) {
				push_audio(s);
			} else {
				audio_eof = 1;
				a_eof = 1;
			}
			did = 1;
		} else if (!video_eof && slots[q_w].state == SLOT_FREE) {
			u64 t0 = gettime();
			plm_frame_t *f = plm_decode_video(plm);
			double ms = us_between(t0, gettime()) / 1000.0;
			if (f) {
				dec_sum += ms;
				dec_count++;
				if (ms > dec_max) {
					dec_max = ms;
				}
				push_frame(f);
			} else {
				video_eof = 1;
			}
			did = 1;
		}
		if (!did) {
			usleep(2000);
		}
	}
	return NULL;
}

static int alloc_slots(int w, int h)
{
	int lw = (w + 15) & ~15;
	int lh = (h + 15) & ~15;
	for (int i = 0; i < SLOT_COUNT; i++) {
		slot_t *s = &slots[i];
		memset(s, 0, sizeof(*s));
		s->lw = lw;
		s->lh = lh;
		s->y = memalign(32, lw * lh);
		s->cb = memalign(32, lw * lh / 4);
		s->cr = memalign(32, lw * lh / 4);
		s->rgb = memalign(32, lw * lh * 2);
		if (!s->y || !s->cb || !s->cr || !s->rgb) {
			return 0;
		}
		GX_InitTexObj(&s->ty, s->y, lw, lh, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObj(&s->tcb, s->cb, lw / 2, lh / 2, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObj(&s->tcr, s->cr, lw / 2, lh / 2, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObj(&s->trgb, s->rgb, lw, lh, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjFilterMode(&s->ty, GX_LINEAR, GX_LINEAR);
		GX_InitTexObjFilterMode(&s->tcb, GX_LINEAR, GX_LINEAR);
		GX_InitTexObjFilterMode(&s->tcr, GX_LINEAR, GX_LINEAR);
		GX_InitTexObjFilterMode(&s->trgb, GX_LINEAR, GX_LINEAR);
	}
	return 1;
}

static void free_slots(void)
{
	for (int i = 0; i < SLOT_COUNT; i++) {
		free(slots[i].y);
		free(slots[i].cb);
		free(slots[i].cr);
		free(slots[i].rgb);
		memset(&slots[i], 0, sizeof(slots[i]));
	}
}

static void draw_video(const slot_t *s)
{
	double da = widescreen ? 16.0 / 9.0 : 4.0 / 3.0;
	double va = (double)s->fw * vid_par / (double)s->fh;
	float qw = 640.0f, qh = 480.0f;
	if (va >= da) {
		qh = (float)(480.0 * da / va);
	} else {
		qw = (float)(640.0 * va / da);
	}
	float x0 = (640.0f - qw) / 2.0f;
	float y0 = (480.0f - qh) / 2.0f;
	float s1 = (float)s->fw / s->lw;
	float t1 = (float)s->fh / s->lh;
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	if (s->mode == MODE_TEV) {
		GX_LoadTexObj((GXTexObj *)&s->ty, GX_TEXMAP0);
		GX_LoadTexObj((GXTexObj *)&s->tcb, GX_TEXMAP1);
		GX_LoadTexObj((GXTexObj *)&s->tcr, GX_TEXMAP2);
		tev_yuv();
	} else {
		GX_LoadTexObj((GXTexObj *)&s->trgb, GX_TEXMAP0);
		tev_single(GX_TEXMAP0, GX_REPLACE);
	}
	quad(x0, y0, x0 + qw, y0 + qh, 0, 0, s1, t1, (GXColor){255, 255, 255, 255});
}

static const char *mode_name(int m)
{
	return m == MODE_TEV ? "TEV" : "CPU";
}

static void scan_device(const char *dev)
{
	char dir[64];
	snprintf(dir, sizeof(dir), "%s:%s", dev, SPIKE_DIR);
	DIR *d = opendir(dir);
	if (!d) {
		return;
	}
	struct dirent *e;
	while ((e = readdir(d)) && entry_count < MAX_FILES) {
		const char *n = e->d_name;
		size_t len = strlen(n);
		if (n[0] == '.' || len < 5 || strcasecmp(n + len - 4, ".mpg") != 0) {
			continue;
		}
		if (e->d_type == DT_DIR) {
			continue;
		}
		entry_t *en = &entries[entry_count++];
		snprintf(en->path, sizeof(en->path), "%s/%s", dir, n);
		snprintf(en->label, sizeof(en->label), "%s: %s", dev, n);
	}
	closedir(d);
}

static int cmp_entry(const void *a, const void *b)
{
	return strcasecmp(((const entry_t *)a)->label, ((const entry_t *)b)->label);
}

static int default_entry(void)
{
	const char *pref[2] = {"sd:", "usb:"};
	for (int p = 0; p < 2; p++) {
		char want[64];
		snprintf(want, sizeof(want), "%s%s/%s", pref[p], SPIKE_DIR, DEFAULT_FILE);
		for (int i = 0; i < entry_count; i++) {
			if (strcasecmp(entries[i].path, want) == 0) {
				return i;
			}
		}
	}
	return 0;
}

static void scan_files(int sd_ok, int usb_ok)
{
	entry_count = 0;
	if (sd_ok) {
		scan_device("sd");
	}
	if (usb_ok) {
		scan_device("usb");
	}
	qsort(entries, entry_count, sizeof(entry_t), cmp_entry);
}

static void open_log(int sd_ok, int usb_ok)
{
	const char *devs[2] = {"sd", "usb"};
	int oks[2] = {sd_ok, usb_ok};
	for (int i = 0; i < 2 && !log_fp; i++) {
		if (!oks[i]) {
			continue;
		}
		char path[96];
		snprintf(path, sizeof(path), "%s:%s", devs[i], SPIKE_DIR);
		DIR *d = opendir(path);
		if (!d) {
			continue;
		}
		closedir(d);
		snprintf(path, sizeof(path), "%s:%s/%s", devs[i], SPIKE_DIR, LOG_NAME);
		log_fp = fopen(path, "a");
	}
}

typedef struct {
	u64 start;
	double excluded_s;
	u32 presented;
	u32 drops;
	u32 window_frames;
	u64 window_start;
	double fps_now;
	u64 seek_req_tick;
	int awaiting_gen;
	double show_ms;
	double seek_ms;
	double seek_pts;
	double seek_target;
	int have_seek;
	double next_log;
	int ended;
	u64 ended_tick;
} pstats_t;

static void stats_reset(pstats_t *ps)
{
	ps->start = gettime();
	ps->excluded_s = 0;
	ps->presented = 0;
	ps->drops = 0;
	ps->window_frames = 0;
	ps->window_start = ps->start;
	ps->fps_now = 0;
	ps->next_log = LOG_INTERVAL;
	ps->ended_tick = ps->start;
	u32 lvl = IRQ_Disable();
	a_underruns = 0;
	IRQ_Restore(lvl);
	stats_reset_req = 1;
}

static double stats_play_s(const pstats_t *ps)
{
	double t = us_between(ps->start, gettime()) / 1000000.0 - ps->excluded_s;
	if (ps->awaiting_gen) {
		t -= us_between(ps->seek_req_tick, gettime()) / 1000000.0;
	} else if (ps->ended) {
		t -= us_between(ps->ended_tick, gettime()) / 1000000.0;
	}
	return t > 0 ? t : 0;
}

static double stats_avg_fps(const pstats_t *ps)
{
	double t = stats_play_s(ps);
	return t > 0.5 ? ps->presented / t : 0;
}

static void log_stats(const char *tag, const char *file, const pstats_t *ps, double clock)
{
	log_line("%s file=%s mode=%s size=%dx%d tv=%dHz aspect=%s t=%.2f play=%.1fs fps_avg=%.2f fps_now=%.1f dec_avg_ms=%.2f dec_max_ms=%.2f cnv_avg_ms=%.2f cnv_max_ms=%.2f underruns=%u drops=%u",
		tag, file, mode_name(cur_mode), vid_w, vid_h, refresh_hz, widescreen ? "16:9" : "4:3", clock,
		stats_play_s(ps), stats_avg_fps(ps), ps->fps_now,
		dec_count ? dec_sum / dec_count : 0.0, (double)dec_max,
		cnv_count ? cnv_sum / cnv_count : 0.0, (double)cnv_max,
		a_underruns, ps->drops);
}

static void request_seek(pstats_t *ps, double target)
{
	if (ps->awaiting_gen) {
		return;
	}
	ps->seek_req_tick = gettime();
	ps->seek_target = target;
	ps->awaiting_gen = seek_gen + 1;
	seek_target_req = target;
	seek_req = 1;
}

static int play(const entry_t *en)
{
	char label[160];
	snprintf(label, sizeof(label), "%s", en->label);
	plm = plm_create_with_filename(en->path);
	if (!plm || !plm_has_headers(plm) || plm_get_num_video_streams(plm) < 1) {
		log_line("error file=%s cannot open or no video stream", en->path);
		if (plm) {
			plm_destroy(plm);
			plm = NULL;
		}
		snprintf(last_result[0], sizeof(last_result[0]), "cannot play %.30s", en->label);
		last_result_lines = 1;
		return 0;
	}
	vid_w = plm_get_width(plm);
	vid_h = plm_get_height(plm);
	vid_par = plm_get_pixel_aspect_ratio(plm);
	if (!(vid_par > 0.1 && vid_par < 10.0)) {
		vid_par = 1.0;
	}
	has_audio = plm_get_num_audio_streams(plm) > 0;
	samplerate = has_audio ? plm_get_samplerate(plm) : 48000;
	if (samplerate <= 0) {
		samplerate = 48000;
	}
	duration = plm_get_duration(plm);
	plm_set_audio_lead_time(plm, 0);
	plm_set_loop(plm, FALSE);
	plm_set_video_decode_callback(plm, on_video, NULL);
	plm_set_audio_decode_callback(plm, on_audio, NULL);
	if (!alloc_slots(vid_w, vid_h)) {
		log_line("error file=%s out of memory for %dx%d", en->path, vid_w, vid_h);
		free_slots();
		plm_destroy(plm);
		plm = NULL;
		return 0;
	}
	log_line("start file=%s size=%dx%d par=%.3f fps=%.3f audio=%d rate=%d duration=%.2f tv=%dHz aspect=%s mode=%s seek_target=%.1f",
		en->path, vid_w, vid_h, vid_par, plm_get_framerate(plm), has_audio, samplerate, duration,
		refresh_hz, widescreen ? "16:9" : "4:3", mode_name(cur_mode), (double)SEEK_TARGET);

	q_r = q_w = 0;
	shown = -1;
	dec_quit = 0;
	seek_req = 0;
	seek_gen = 0;
	seek_done_gen = 0;
	in_seek = 0;
	video_eof = 0;
	audio_eof = 0;
	audio_reset();
	wall_running = 0;
	prefill_audio();
	if (has_audio) {
		memset(audio_bufs, 0, sizeof(audio_bufs));
		DCFlushRange(audio_bufs, sizeof(audio_bufs));
		audio_buf_idx = 1;
		ASND_SetVoice(0, VOICE_STEREO_16BIT, samplerate, 0, audio_bufs[0], AUDIO_CHUNK_BYTES, 255, 255, audio_cb);
		audio_go();
	} else {
		wall_base = 0;
		wall_tick = gettime();
		wall_running = 1;
	}

	pstats_t ps;
	memset(&ps, 0, sizeof(ps));
	stats_reset(&ps);
	dec_sum = dec_max = cnv_sum = cnv_max = 0;
	dec_count = cnv_count = 0;
	stats_reset_req = 0;

	lwp_t th;
	void *stack = memalign(32, DECODE_STACK);
	LWP_CreateThread(&th, decode_thread, NULL, stack, DECODE_STACK, DECODE_PRIO);

#ifdef AUTOTEST
	auto_start = gettime();
	auto_step = 0;
#endif
	int result = 0;
	int overlay = 1;
	while (1) {
		u32 b = read_buttons();
		if (b & BTN_HOME) {
			result = -1;
			break;
		}
		if (b & BTN_B) {
			break;
		}
		if (b & BTN_A) {
			overlay = !overlay;
		}
		if (b & BTN_1) {
			request_seek(&ps, SEEK_TARGET);
		}
		if (b & BTN_MINUS) {
			request_seek(&ps, 0.0);
		}
		if (b & BTN_PLUS) {
			seek_exact = !seek_exact;
			log_line("seek_exact=%d", seek_exact);
		}
		if (b & BTN_2) {
			log_stats("mode-switch", label, &ps, av_clock());
			cur_mode = cur_mode == MODE_TEV ? MODE_CPU : MODE_TEV;
			stats_reset(&ps);
			log_line("mode=%s stats reset", mode_name(cur_mode));
		}

		if (ps.awaiting_gen && seek_done_gen == ps.awaiting_gen && !seek_ok) {
			log_line("seek file=%s target=%.3f exact=%d ok=0 seek_ms=%.1f", label, ps.seek_target, seek_exact, (double)seek_ms);
			ps.excluded_s += us_between(ps.seek_req_tick, gettime()) / 1000000.0;
			ps.awaiting_gen = 0;
		}

		double clock = av_clock();
		LWP_MutexLock(q_lock);
		while (clock >= 0) {
			slot_t *s = &slots[q_r];
			if (s->state != SLOT_READY || s->time > clock + DUE_SLACK) {
				break;
			}
			int nx = (q_r + 1) % SLOT_COUNT;
			if (slots[nx].state == SLOT_READY && slots[nx].time <= clock + DUE_SLACK && s->gen == slots[nx].gen) {
				s->state = SLOT_FREE;
				q_r = nx;
				ps.drops++;
				continue;
			}
			if (shown >= 0) {
				slots[shown].state = SLOT_FREE;
			}
			s->state = SLOT_SHOWN;
			shown = q_r;
			q_r = nx;
			ps.presented++;
			ps.window_frames++;
			if (ps.awaiting_gen && s->gen == ps.awaiting_gen && seek_done_gen == ps.awaiting_gen) {
				ps.show_ms = us_between(ps.seek_req_tick, gettime()) / 1000.0;
				ps.excluded_s += ps.show_ms / 1000.0;
				ps.seek_ms = seek_ms;
				ps.seek_pts = seek_pts;
				ps.have_seek = 1;
				ps.awaiting_gen = 0;
				log_line("seek file=%s target=%.3f exact=%d ok=%d seek_ms=%.1f show_ms=%.1f pts_after=%.3f error=%.3f",
					label, ps.seek_target, seek_exact, seek_ok, ps.seek_ms, ps.show_ms, ps.seek_pts, ps.seek_pts - ps.seek_target);
			}
			break;
		}
		LWP_MutexUnlock(q_lock);

		u64 now = gettime();
		double wdt = us_between(ps.window_start, now) / 1000000.0;
		if (wdt >= 1.0) {
			ps.fps_now = ps.window_frames / wdt;
			ps.window_frames = 0;
			ps.window_start = now;
		}
		double play_s = stats_play_s(&ps);
		if (play_s >= ps.next_log && !ps.ended) {
			log_stats("stats", label, &ps, clock);
			ps.next_log += LOG_INTERVAL;
		}
		if (!ps.ended && video_eof && !ps.awaiting_gen && !seek_req) {
			int empty = 1;
			for (int i = 0; i < SLOT_COUNT; i++) {
				if (slots[i].state == SLOT_READY) {
					empty = 0;
				}
			}
			if (empty && (!has_audio || audio_eof)) {
				log_stats("end", label, &ps, clock);
				ps.ended = 1;
				ps.ended_tick = gettime();
			}
		}
		if (ps.ended && (seek_req || ps.awaiting_gen)) {
			ps.excluded_s += us_between(ps.ended_tick, ps.awaiting_gen ? ps.seek_req_tick : gettime()) / 1000000.0;
			ps.ended = 0;
		}

		frame_begin();
		if (shown >= 0) {
			draw_video(&slots[shown]);
		}
		if (overlay) {
			GXColor bg = {0, 0, 0, 160};
			GXColor fg = {255, 255, 255, 255};
			GXColor hi = {255, 220, 64, 255};
			float x = 24, y = 24;
			draw_box(x - 8, y - 4, x + 26 * CHAR_W + 8, y + 9 * CHAR_H + 4, bg);
			draw_textf(x, y, hi, "%s %dx%d %dHz %s", mode_name(cur_mode), vid_w, vid_h, refresh_hz, widescreen ? "16:9" : "4:3");
			y += CHAR_H;
			draw_textf(x, y, fg, "fps %.1f avg %.2f", ps.fps_now, stats_avg_fps(&ps));
			y += CHAR_H;
			draw_textf(x, y, fg, "dec %.1f max %.1f ms", dec_count ? dec_sum / dec_count : 0.0, (double)dec_max);
			y += CHAR_H;
			draw_textf(x, y, fg, "cnv %.1f max %.1f ms", cnv_count ? cnv_sum / cnv_count : 0.0, (double)cnv_max);
			y += CHAR_H;
			draw_textf(x, y, fg, "underrun %u drop %u", a_underruns, ps.drops);
			y += CHAR_H;
			if (ps.awaiting_gen) {
				draw_text(x, y, hi, "seeking...");
			} else if (ps.have_seek) {
				draw_textf(x, y, fg, "seek %.0f show %.0f ms", ps.seek_ms, ps.show_ms);
			} else {
				draw_text(x, y, fg, "seek -");
			}
			y += CHAR_H;
			if (ps.have_seek) {
				draw_textf(x, y, fg, "to %.2f got %.3f", ps.seek_target, ps.seek_pts);
			} else {
				draw_textf(x, y, fg, "1 seeks to %.0f s", (double)SEEK_TARGET);
			}
			y += CHAR_H;
			draw_textf(x, y, fg, "t %.1f/%.0f exact %s", clock < 0 ? 0.0 : clock, duration, seek_exact ? "on" : "off");
			y += CHAR_H;
			draw_text(x, y, ps.ended ? hi : fg, ps.ended ? "END  B=list" : "A hide 2 TEV/CPU");
		}
		frame_end();
	}

	log_stats("stop", label, &ps, av_clock());
	snprintf(last_result[0], sizeof(last_result[0]), "%.28s %s", en->label, mode_name(cur_mode));
	snprintf(last_result[1], sizeof(last_result[1]), "fps avg %.2f underruns %u", stats_avg_fps(&ps), a_underruns);
	snprintf(last_result[2], sizeof(last_result[2]), "dec avg %.1f max %.1f ms", dec_count ? dec_sum / dec_count : 0.0, (double)dec_max);
	if (ps.have_seek) {
		snprintf(last_result[3], sizeof(last_result[3]), "seek %.0f ms pts %.3f", ps.seek_ms, ps.seek_pts);
		last_result_lines = 4;
	} else {
		last_result_lines = 3;
	}

	dec_quit = 1;
	LWP_JoinThread(th, NULL);
	free(stack);
	if (has_audio) {
		ASND_StopVoice(0);
	}
	audio_reset();
	plm_destroy(plm);
	plm = NULL;
	free_slots();
	return result;
}

static int list_screen(int *cursor, int sd_ok, int usb_ok)
{
#ifdef AUTOTEST
	if (entry_count > 0) {
		return *cursor;
	}
#endif
	while (1) {
		u32 b = read_buttons();
		if (b & BTN_HOME) {
			return -1;
		}
		if (entry_count > 0) {
			if (b & BTN_UP) {
				*cursor = (*cursor + entry_count - 1) % entry_count;
			}
			if (b & BTN_DOWN) {
				*cursor = (*cursor + 1) % entry_count;
			}
			if (b & BTN_A) {
				return *cursor;
			}
		}
		if (b & BTN_2) {
			cur_mode = cur_mode == MODE_TEV ? MODE_CPU : MODE_TEV;
		}

		frame_begin();
		GXColor fg = {255, 255, 255, 255};
		GXColor dim = {160, 160, 160, 255};
		GXColor hi = {255, 220, 64, 255};
		GXColor sel = {40, 80, 160, 255};
		float x = 32, y = 24;
		draw_text(x, y, hi, "cable-twii video spike");
		y += CHAR_H;
		draw_textf(x, y, dim, "sd:%s usb:%s %dHz %s %s", sd_ok ? "ok" : "--", usb_ok ? "ok" : "--", refresh_hz, widescreen ? "16:9" : "4:3", mode_name(cur_mode));
		y += CHAR_H + 8;
		if (entry_count == 0) {
			draw_text(x, y, fg, "no .mpg files found in");
			y += CHAR_H;
			draw_text(x, y, fg, "sd:/cable-twii-spike/ or");
			y += CHAR_H;
			draw_text(x, y, fg, "usb:/cable-twii-spike/");
			y += CHAR_H;
		} else {
			int first = 0;
			int visible = 6;
			if (*cursor >= visible) {
				first = *cursor - visible + 1;
			}
			for (int i = first; i < entry_count && i < first + visible; i++) {
				char line[40];
				snprintf(line, sizeof(line), "%.36s", entries[i].label);
				if (i == *cursor) {
					draw_box(x - 4, y, x + 37 * CHAR_W, y + CHAR_H, sel);
				}
				draw_text(x, y, i == *cursor ? hi : fg, line);
				y += CHAR_H;
			}
		}
		y = 480 - 24 - CHAR_H * (2 + last_result_lines);
		for (int i = 0; i < last_result_lines; i++) {
			draw_text(x, y, dim, last_result[i]);
			y += CHAR_H;
		}
		draw_text(x, y, fg, "A play  2 TEV/CPU");
		y += CHAR_H;
		draw_text(x, y, fg, "HOME exit");
		frame_end();
	}
}

int main(int argc, char **argv)
{
	LWP_SetThreadPriority(LWP_GetSelf(), MAIN_PRIO);
	video_init();
	SYS_STDIO_Report(true);
	SYS_SetResetCallback(reset_cb);
	SYS_SetPowerCallback(power_cb);
	WPAD_Init();
	PAD_Init();
	ASND_Init();
	ASND_Pause(0);
	LWP_MutexInit(&log_lock, false);
	LWP_MutexInit(&q_lock, false);

	int sd_ok = fatMountSimple("sd", &__io_wiisd);
	int usb_ok = fatMountSimple("usb", &__io_usbstorage);
	open_log(sd_ok, usb_ok);
	log_line("cable-twii video spike tv=%dHz tvmode=%u aspect=%s efb=%dx%d xfb=%d sd=%d usb=%d seek_target=%.1f",
		refresh_hz, VIDEO_GetCurrentTvMode(), widescreen ? "16:9" : "4:3", rmode->fbWidth, rmode->efbHeight,
		rmode->xfbHeight, sd_ok, usb_ok, (double)SEEK_TARGET);
	scan_files(sd_ok, usb_ok);
	for (int i = 0; i < entry_count; i++) {
		log_line("found %s", entries[i].path);
	}

	int cursor = default_entry();
	while (1) {
		int pick = list_screen(&cursor, sd_ok, usb_ok);
		if (pick < 0) {
			break;
		}
		if (play(&entries[pick]) < 0) {
			break;
		}
	}

	log_line("exit");
	if (log_fp) {
		fclose(log_fp);
	}
	ASND_End();
	WPAD_Shutdown();
	if (sd_ok) {
		fatUnmount("sd");
	}
	if (usb_ok) {
		fatUnmount("usb");
	}
	exit(0);
	return 0;
}
