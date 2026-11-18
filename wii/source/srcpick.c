#include "srcpick.h"

#include "net/addr.h"

bool sp_try_server(const ctw_settings *s)
{
    srv_addr a;
    return s->source == SRC_SERVER && addr_parse(s->server, &a) == 0;
}

sp_outcome sp_after_server(srv_result r, bool drive_found)
{
    if (r == SRV_OK) return SP_SERVER;
    return drive_found ? SP_DRIVE_WITH_NOTICE : SP_FAIL;
}
