#include "yuvgx.h"

#include <malloc.h>
#include <string.h>
#include <gccore.h>

typedef struct {
    uint8_t *y;
    uint8_t *cb;
    uint8_t *cr;
    GXTexObj ty, tcb, tcr;
} yg_slot;

static yg_slot slots[YG_SLOTS];
static int tex_w, tex_h;

static void tile_i8(uint8_t *dst, const uint8_t *src, int w, int h, int stride)
{
    uint32_t *d = (uint32_t *)dst;
    for (int y = 0; y < h; y += 4) {
        const uint8_t *r0 = src + y * stride;
        const uint8_t *r1 = r0 + stride;
        const uint8_t *r2 = r1 + stride;
        const uint8_t *r3 = r2 + stride;
        for (int x = 0; x < w; x += 8) {
            const uint32_t *a = (const uint32_t *)(r0 + x);
            const uint32_t *b = (const uint32_t *)(r1 + x);
            const uint32_t *c = (const uint32_t *)(r2 + x);
            const uint32_t *e = (const uint32_t *)(r3 + x);
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

int yg_init(int w, int h)
{
    tex_w = (w + 15) & ~15;
    tex_h = (h + 15) & ~15;
    for (int i = 0; i < YG_SLOTS; i++) {
        yg_slot *s = &slots[i];
        memset(s, 0, sizeof(*s));
        s->y = memalign(32, tex_w * tex_h);
        s->cb = memalign(32, tex_w * tex_h / 4);
        s->cr = memalign(32, tex_w * tex_h / 4);
        if (s->y == NULL || s->cb == NULL || s->cr == NULL) {
            yg_free();
            return -1;
        }
        GX_InitTexObj(&s->ty, s->y, tex_w, tex_h, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObj(&s->tcb, s->cb, tex_w / 2, tex_h / 2, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObj(&s->tcr, s->cr, tex_w / 2, tex_h / 2, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjFilterMode(&s->ty, GX_LINEAR, GX_LINEAR);
        GX_InitTexObjFilterMode(&s->tcb, GX_LINEAR, GX_LINEAR);
        GX_InitTexObjFilterMode(&s->tcr, GX_LINEAR, GX_LINEAR);
    }
    return 0;
}

void yg_free(void)
{
    for (int i = 0; i < YG_SLOTS; i++) {
        free(slots[i].y);
        free(slots[i].cb);
        free(slots[i].cr);
        memset(&slots[i], 0, sizeof(slots[i]));
    }
}

void yg_upload(int slot, const uint8_t *y, const uint8_t *cb, const uint8_t *cr, int w, int h)
{
    yg_slot *s = &slots[slot];
    tile_i8(s->y, y, w, h, w);
    tile_i8(s->cb, cb, w / 2, h / 2, w / 2);
    tile_i8(s->cr, cr, w / 2, h / 2, w / 2);
    DCFlushRange(s->y, tex_w * tex_h);
    DCFlushRange(s->cb, tex_w * tex_h / 4);
    DCFlushRange(s->cr, tex_w * tex_h / 4);
}

static void tev_yuv_stage(uint8_t stage, uint8_t texcoord, uint32_t texmap, uint8_t ksel, uint8_t d, uint8_t op, uint8_t scale, uint8_t clamp)
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

static void setup_tev_yuv(void)
{
    GXColorS10 offset = { -112, 68, -137, 255 };
    GX_SetTevColorS10(GX_TEVREG0, offset);
    GX_SetTevKColor(GX_KCOLOR0, (GXColor){ 204, 0, 0, 148 });
    GX_SetTevKColor(GX_KCOLOR1, (GXColor){ 0, 0, 255, 0 });
    GX_SetTevKColor(GX_KCOLOR2, (GXColor){ 0, 50, 0, 0 });
    GX_SetTevKColor(GX_KCOLOR3, (GXColor){ 0, 104, 0, 0 });
    GX_SetNumTexGens(2);
    GX_SetNumTevStages(5);
    tev_yuv_stage(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_TEV_KCSEL_K0_A, GX_CC_C0, GX_TEV_ADD, GX_CS_SCALE_1, GX_FALSE);
    tev_yuv_stage(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_TEV_KCSEL_K1, GX_CC_CPREV, GX_TEV_ADD, GX_CS_SCALE_1, GX_FALSE);
    tev_yuv_stage(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP1, GX_TEV_KCSEL_K2, GX_CC_CPREV, GX_TEV_SUB, GX_CS_SCALE_1, GX_FALSE);
    tev_yuv_stage(GX_TEVSTAGE3, GX_TEXCOORD1, GX_TEXMAP2, GX_TEV_KCSEL_K0, GX_CC_CPREV, GX_TEV_ADD, GX_CS_SCALE_1, GX_FALSE);
    tev_yuv_stage(GX_TEVSTAGE4, GX_TEXCOORD1, GX_TEXMAP2, GX_TEV_KCSEL_K3, GX_CC_CPREV, GX_TEV_SUB, GX_CS_SCALE_2, GX_TRUE);
}

void yg_draw(int slot, int frame_w, int frame_h, float x, float y, float w, float h)
{
    yg_slot *s = &slots[slot];
    float s1 = (float)frame_w / (float)tex_w;
    float t1 = (float)frame_h / (float)tex_h;

    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    GX_LoadTexObj(&s->ty, GX_TEXMAP0);
    GX_LoadTexObj(&s->tcb, GX_TEXMAP1);
    GX_LoadTexObj(&s->tcr, GX_TEXMAP2);
    setup_tev_yuv();
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(x, y, 0.0f);
    GX_Color1u32(0xFFFFFFFF);
    GX_TexCoord2f32(0, 0);
    GX_Position3f32(x + w, y, 0.0f);
    GX_Color1u32(0xFFFFFFFF);
    GX_TexCoord2f32(s1, 0);
    GX_Position3f32(x + w, y + h, 0.0f);
    GX_Color1u32(0xFFFFFFFF);
    GX_TexCoord2f32(s1, t1);
    GX_Position3f32(x, y + h, 0.0f);
    GX_Color1u32(0xFFFFFFFF);
    GX_TexCoord2f32(0, t1);
    GX_End();

    GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
    GX_SetNumTexGens(1);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}
