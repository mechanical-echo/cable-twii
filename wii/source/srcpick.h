#ifndef CTW_SRCPICK_H
#define CTW_SRCPICK_H

#include <stdbool.h>
#include <stddef.h>
#include "net/srvlib.h"
#include "settings.h"
#include "volume.h"

typedef enum { SP_SERVER, SP_DRIVE_WITH_NOTICE, SP_FAIL } sp_outcome;

bool sp_try_server(const ctw_settings *s);
sp_outcome sp_after_server(srv_result r, bool drive_found);
const char *sp_fallback_notice(vol_source source);
void sp_fail_body(srv_result r, const char *address, char *out, size_t out_size);
void sp_connecting(const char *address, char *out, size_t out_size);

#endif
