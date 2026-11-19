#include "video.h"

#include <gccore.h>
#include <grrlib.h>

#include "vmode.h"

static void rescale(void)
{
    vm_plan p = vm_plan_for(rmode->efbHeight, rmode->xfbHeight);
    if (!p.rescale || rmode->aa) return;

    rmode->efbHeight = (u16)p.efb_height;
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    u32 xfb_height = GX_SetDispCopyYScale(256.0f / ((f32)p.scale_reg + 0.5f));
    GX_SetDispCopyDst(rmode->fbWidth, xfb_height);

    Mtx44 ortho;
    guOrtho(ortho, 0.0f, rmode->efbHeight, 0.0f, rmode->fbWidth, 0.0f, 1000.0f);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);
    GX_SetViewport(0.0f, 0.0f, rmode->fbWidth, rmode->efbHeight, 0.0f, 1.0f);
    GRRLIB_ClipReset();
}

void video_init(void)
{
    GRRLIB_Init();
    rescale();
    for (int i = 0; i < 2; i++) {
        VIDEO_ClearFrameBuffer(rmode, xfb[i], COLOR_BLACK);
    }
    for (int i = 0; i < 2; i++) {
        GRRLIB_FillScreen(0x000000FF);
        GRRLIB_Render();
    }
}
