#ifndef CTW_UI_ICONS_H
#define CTW_UI_ICONS_H

#define ICON_CACHE_CAPACITY 24
#define ICON_LOADS_PER_FRAME 2

void icons_init(void);
void icons_frame(void);
void icons_draw(const char *path, float x, float y, float size);

#endif
