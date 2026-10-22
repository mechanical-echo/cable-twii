#ifndef CTW_QUIT_H
#define CTW_QUIT_H

#include <stdbool.h>

typedef enum { QUIT_LOADER, QUIT_POWER_OFF, QUIT_WII_MENU } quit_route;

#define QUIT_STUB_OFFSET 4

bool quit_stub_present(const unsigned char *stub_area);
quit_route quit_route_for(bool stub_present, bool emulator);

#endif
