#include <string.h>
#include "quit.h"

bool quit_stub_present(const unsigned char *stub_area)
{
    return memcmp(stub_area + QUIT_STUB_OFFSET, "STUBHAXX", 8) == 0;
}

quit_route quit_route_for(quit_dest dest, bool stub_present, bool emulator)
{
    if (emulator || dest == QUIT_DEST_POWER_OFF) return QUIT_POWER_OFF;
    if (dest == QUIT_DEST_HOMEBREW && stub_present) return QUIT_LOADER;
    return QUIT_WII_MENU;
}
