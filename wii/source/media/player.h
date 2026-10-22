#ifndef CTW_MEDIA_PLAYER_H
#define CTW_MEDIA_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { PL_IDLE, PL_SEEKING, PL_PLAYING, PL_ENDED, PL_ERROR } pl_state;

int pl_open(const char *path, uint32_t offset_ms);
void pl_close(void);
pl_state pl_status(void);
void pl_draw(float x, float y, float w, float h, float alpha);
void pl_set_volume(int volume_0_100);
uint32_t pl_position_ms(void);
int pl_video_size(int *w, int *h);
void pl_set_widescreen(bool widescreen);
#ifdef CTW_DEBUG_OVERLAY
void pl_debug_counts(uint32_t *decoded, uint32_t *shown_frames, uint32_t *dropped);
#endif

#endif
