#ifndef CTW_NET_HTTP_H
#define CTW_NET_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "net/addr.h"

#define HTTP_E_CONNECT (-1)
#define HTTP_E_TIMEOUT (-2)
#define HTTP_E_STATUS (-3)
#define HTTP_E_PROTO (-4)
#define HTTP_E_MEM (-5)
#define HTTP_E_TOO_LARGE (-6)

typedef struct {
    char *data;
    size_t len;
    int status;
} http_body;

int http_get(const srv_addr *a, const char *path, size_t max_bytes, uint32_t timeout_ms, const volatile bool *cancel, http_body *out);
void http_body_free(http_body *b);

typedef struct {
    srv_addr addr;
    char path[512];
    int fd;
    uint64_t length;
    uint64_t remaining;
    uint32_t timeout_ms;
    const volatile bool *cancel;
} http_stream;

int hs_open(http_stream *s, const srv_addr *a, const char *path, uint32_t timeout_ms, const volatile bool *cancel);
int hs_start(http_stream *s, uint64_t offset);
long hs_read(http_stream *s, void *dst, size_t len);
void hs_close(http_stream *s);

#endif
