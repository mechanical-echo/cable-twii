#include "video.h"

#include <gccore.h>
#include <grrlib.h>

#include "vmode.h"

void video_init(void)
{
    GRRLIB_Init();
    vm_plan p = vm_plan_for(rmode->efbHeight, rmode->xfbHeight);
    if (!p.rescale) return;

    rmode->efbHeight = (u16)p.efb_height;
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfb_height = GX_SetDispCopyYScale(yscale);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfb_height);

    Mtx44 ortho;
    guOrtho(ortho, 0.0f, rmode->efbHeight, 0.0f, rmode->fbWidth, 0.0f, 1000.0f);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);
    GX_SetViewport(0.0f, 0.0f, rmode->fbWidth, rmode->efbHeight, 0.0f, 1.0f);
    GRRLIB_ClipReset();
}
