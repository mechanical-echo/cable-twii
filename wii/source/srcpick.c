#include "srcpick.h"

#include <stdio.h>
#include "net/addr.h"

bool sp_try_server(const ctw_settings *s)
{
    srv_addr a;
    return s->source == SRC_SERVER && addr_parse(s->server, &a) == 0;
}

sp_outcome sp_after_server(srv_result r, bool reload, bool drive_found)
{
    if (r == SRV_OK) return SP_SERVER;
    if (reload) return SP_KEEP;
    return drive_found ? SP_DRIVE_WITH_NOTICE : SP_FAIL;
}

const char *sp_reload_notice(srv_result r, bool manual)
{
    if (r == SRV_OK) return "Channels updated";
    return manual ? "Can't reach the server" : NULL;
}

const char *sp_fallback_notice(vol_source source)
{
    return source == VOL_USB ? "Server not reachable, playing from USB" : "Server not reachable, playing from the SD card";
}

void sp_fail_body(srv_result r, const char *address, char *out, size_t out_size)
{
    if (r == SRV_NO_NETWORK) {
        srv_test_message(r, address, "", out, out_size);
        return;
    }
    snprintf(out, out_size, "cable-twii couldn't connect to %s. Check that the server is running and the Wii is online, or change the address in Settings.", address);
}

void sp_connecting(const char *address, char *out, size_t out_size)
{
    snprintf(out, out_size, "Connecting to %s\xE2\x80\xA6", address);
}
