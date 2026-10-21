#include "sfx.h"

#include <asndlib.h>

#include "sfx_hover_pcm.h"
#include "sfx_select_pcm.h"
#include "sfx_back_pcm.h"
#include "sfx_channel_pcm.h"

typedef struct {
    const u8 *data;
    u32 size;
} clip;

static clip clips[4];
static s32 next_voice;

void sfx_init(void)
{
    clips[SFX_HOVER] = (clip){ sfx_hover_pcm, sfx_hover_pcm_size };
    clips[SFX_SELECT] = (clip){ sfx_select_pcm, sfx_select_pcm_size };
    clips[SFX_BACK] = (clip){ sfx_back_pcm, sfx_back_pcm_size };
    clips[SFX_CHANNEL] = (clip){ sfx_channel_pcm, sfx_channel_pcm_size };
    ASND_Init();
    ASND_Pause(0);
}

void sfx_play(sfx_id id)
{
    clip c = clips[id];
    s32 voice = next_voice;
    next_voice = (next_voice + 1) % 8;
    ASND_StopVoice(voice);
    ASND_SetVoice(voice, VOICE_MONO_16BIT_BE, 48000, 0, (void *)c.data, (s32)c.size, 255, 255, NULL);
}
