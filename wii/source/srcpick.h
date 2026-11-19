#ifndef CTW_SRCPICK_H
#define CTW_SRCPICK_H

#include <stdbool.h>
#include <stddef.h>
#include "net/srvlib.h"
#include "settings.h"
#include "volume.h"

typedef enum { SP_SERVER, SP_DRIVE_WITH_NOTICE, SP_FAIL, SP_KEEP } sp_outcome;
typedef enum { SP_LINE_NONE, SP_LINE_BUSY, SP_LINE_OK, SP_LINE_FAIL } sp_line;

bool sp_try_server(const ctw_settings *s);
sp_outcome sp_after_server(srv_result r, bool reload, bool drive_found);
const char *sp_reload_notice(srv_result r, bool manual);
const char *sp_fallback_notice(vol_source source);
void sp_fail_body(srv_result r, const char *address, char *out, size_t out_size);
void sp_connecting(const char *address, char *out, size_t out_size);
sp_line sp_test_start(const char *typed, srv_addr *addr, char *name, size_t name_size, char *out, size_t out_size);
sp_line sp_test_result(srv_result r, const char *name, const char *server_name, char *out, size_t out_size);

#endif
