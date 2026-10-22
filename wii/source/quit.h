#ifndef CTW_QUIT_H
#define CTW_QUIT_H

#include <stdbool.h>

typedef enum { QUIT_LOADER, QUIT_POWER_OFF, QUIT_WII_MENU } quit_route;
typedef enum { QUIT_DEST_WII_MENU, QUIT_DEST_HOMEBREW, QUIT_DEST_POWER_OFF } quit_dest;

#define QUIT_STUB_OFFSET 4

bool quit_stub_present(const unsigned char *stub_area);
quit_route quit_route_for(quit_dest dest, bool stub_present, bool emulator);

#endif
