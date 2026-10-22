#ifndef CTW_MEDIA_AUDIO_H
#define CTW_MEDIA_AUDIO_H

void au_init(void);
void au_reset(void);
void au_push(const float *interleaved, unsigned int count, double pts);
void au_end(void);
void au_go(void);
double au_clock(int sample_rate);
unsigned int au_ring_free(void);
unsigned int au_ring_fill(void);
void au_set_volume(int volume_0_100);
#ifdef CTW_DEBUG_OVERLAY
unsigned int au_underrun_count(void);
#endif

#endif
