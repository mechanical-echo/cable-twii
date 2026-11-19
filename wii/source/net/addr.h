#ifndef CTW_NET_ADDR_H
#define CTW_NET_ADDR_H

#include <stddef.h>
#include <stdint.h>

#define ADDR_DEFAULT_PORT 4747
#define ADDR_HOST_BYTES 64
#define ADDR_REL_MAX 1023
#define ADDR_URL_BYTES (sizeof "http://" + ADDR_HOST_BYTES + sizeof ":65535/wii/v1/files/" + ADDR_REL_MAX)
#define ADDR_PATH_BYTES (sizeof "/wii/v1/files/" + 3 * ADDR_REL_MAX)

typedef struct {
    char host[ADDR_HOST_BYTES];
    uint16_t port;
} srv_addr;

int addr_parse(const char *typed, srv_addr *out);
void addr_format(const srv_addr *a, char *out, size_t out_size);
int url_encode_path(const char *rel, char *out, size_t out_size);
int url_split(const char *url, srv_addr *addr, char *path, size_t path_size);
void srv_root(const srv_addr *a, char *out, size_t out_size);

#endif
