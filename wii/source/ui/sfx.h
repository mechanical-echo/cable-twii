#ifndef CTW_UI_SFX_H
#define CTW_UI_SFX_H

typedef enum { SFX_HOVER, SFX_SELECT, SFX_BACK, SFX_CHANNEL } sfx_id;

void sfx_init(void);
void sfx_play(sfx_id id);

#endif
