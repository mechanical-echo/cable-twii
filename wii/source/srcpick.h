#ifndef CTW_SRCPICK_H
#define CTW_SRCPICK_H

#include <stdbool.h>
#include "net/srvlib.h"
#include "settings.h"

typedef enum { SP_SERVER, SP_DRIVE_WITH_NOTICE, SP_FAIL } sp_outcome;

bool sp_try_server(const ctw_settings *s);
sp_outcome sp_after_server(srv_result r, bool drive_found);

#endif
