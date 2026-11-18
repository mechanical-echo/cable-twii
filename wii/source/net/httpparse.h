#ifndef CTW_NET_HTTPPARSE_H
#define CTW_NET_HTTPPARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int status;
    int64_t content_length;
    int64_t range_start;
    int64_t range_end;
    int64_t total;
    bool gzip;
    bool keep_alive;
} hp_response;

long hp_parse_head(const char *buf, size_t len, hp_response *out);

#endif
