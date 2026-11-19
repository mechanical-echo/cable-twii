#ifndef CTW_NET_SRVLIB_H
#define CTW_NET_SRVLIB_H

#include <stddef.h>
#include <stdint.h>
#include "ctw/ctw.h"
#include "net/addr.h"

typedef struct {
    int version;
    char name[64];
    char revision[64];
} srv_info;

typedef enum { SRV_OK, SRV_NO_NETWORK, SRV_NO_ANSWER, SRV_NOT_CABLE_TWII, SRV_BAD_LIBRARY } srv_result;

int srv_info_parse(const char *toml, srv_info *out);
srv_result srv_fetch_info(const srv_addr *a, uint32_t timeout_ms, srv_info *out);
srv_result srv_load_db(const srv_addr *a, uint32_t timeout_ms, ctw_db **out, ctw_issues *issues, srv_info *info);
void srv_test_message(srv_result r, const char *address, const char *name, char *out, size_t out_size);

#endif
