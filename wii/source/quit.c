#include <string.h>
#include "quit.h"

bool quit_stub_present(const unsigned char *stub_area)
{
    return memcmp(stub_area + QUIT_STUB_OFFSET, "STUBHAXX", 8) == 0;
}

quit_route quit_route_for(bool stub_present, bool emulator)
{
    if (emulator) return QUIT_POWER_OFF;
    if (stub_present) return QUIT_LOADER;
    return QUIT_WII_MENU;
}
