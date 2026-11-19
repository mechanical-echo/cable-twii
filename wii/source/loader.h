#ifndef CTW_LOADER_H
#define CTW_LOADER_H

#include <stdbool.h>
#include <stdint.h>
#include "ctw/ctw.h"
#include "net/addr.h"
#include "net/srvlib.h"

typedef enum { LD_IDLE, LD_RUNNING, LD_DONE } ld_state;

void ld_load(const srv_addr *a);
void ld_info(const srv_addr *a);
ld_state ld_poll(void);
srv_result ld_take_db(ctw_db **db, ctw_issues *issues, srv_info *info);
srv_result ld_take_info(srv_info *info);
void ld_drop(void);
#ifdef CTW_DEBUG_OVERLAY
bool ld_debug_times(uint32_t *net_up_ms, uint32_t *load_ms);
#endif

#endif
