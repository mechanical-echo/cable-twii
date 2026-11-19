#include "net/http.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ZLIB_CONST
#include <zlib.h>
#include "net/httpparse.h"
#include "net/sock.h"

#define HEAD_MAX (ADDR_PATH_BYTES + 1024)
#define GZIP_MIN 18

static int sock_error(long r)
{
    return r == SK_E_TIMEOUT ? HTTP_E_TIMEOUT : HTTP_E_CONNECT;
}

static size_t head_room(const char *buf, size_t n)
{
    for (size_t k = 3; k > 0; k--) {
        if (n >= k && memcmp(buf + n - k, "\r\n\r\n", k) == 0) return 4 - k;
    }
    return 4;
}

static int exchange(int fd, const srv_addr *a, const char *path, const char *extra, uint32_t timeout_ms, const volatile bool *cancel, hp_response *r)
{
    char buf[HEAD_MAX];
    int n = snprintf(buf, sizeof buf, "GET %s HTTP/1.1\r\nHost: %s:%u\r\n%s\r\n", path, a->host, (unsigned)a->port, extra);
    if (n < 0 || (size_t)n >= sizeof buf) return HTTP_E_PROTO;
    int sent = sk_send_all(fd, buf, (size_t)n, timeout_ms, cancel);
    if (sent < 0) return sock_error(sent);
    size_t got = 0;
    for (;;) {
        size_t want = head_room(buf, got);
        if (got + want > sizeof buf) return HTTP_E_PROTO;
        long k = sk_recv(fd, buf + got, want, timeout_ms, cancel);
        if (k <= 0) return sock_error(k);
        got += (size_t)k;
        long head = hp_parse_head(buf, got, r);
        if (head < 0) return HTTP_E_PROTO;
        if (head > 0) return 0;
    }
}

static int recv_all(int fd, char *dst, size_t n, uint32_t timeout_ms, const volatile bool *cancel)
{
    while (n > 0) {
        long k = sk_recv(fd, dst, n, timeout_ms, cancel);
        if (k <= 0) return sock_error(k);
        dst += k;
        n -= (size_t)k;
    }
    return 0;
}

static int gunzip(const unsigned char *src, size_t n, size_t max_bytes, http_body *out)
{
    if (n < GZIP_MIN) return HTTP_E_PROTO;
    uint32_t size = (uint32_t)src[n - 4] | (uint32_t)src[n - 3] << 8 | (uint32_t)src[n - 2] << 16 | (uint32_t)src[n - 1] << 24;
    if (size > max_bytes || size == UINT32_MAX) return HTTP_E_TOO_LARGE;
    char *dst = malloc((size_t)size + 1);
    if (dst == NULL) return HTTP_E_MEM;
    z_stream z;
    memset(&z, 0, sizeof z);
    if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) {
        free(dst);
        return HTTP_E_MEM;
    }
    z.next_in = src;
    z.avail_in = (uInt)n;
    z.next_out = (Bytef *)dst;
    z.avail_out = (uInt)size;
    bool ok = inflate(&z, Z_FINISH) == Z_STREAM_END && z.total_out == size;
    inflateEnd(&z);
    if (!ok) {
        free(dst);
        return HTTP_E_PROTO;
    }
    dst[size] = '\0';
    out->data = dst;
    out->len = size;
    return 0;
}

static int read_body(int fd, const hp_response *r, size_t max_bytes, uint32_t timeout_ms, const volatile bool *cancel, http_body *out)
{
    if (r->status != 200) return HTTP_E_STATUS;
    if (r->content_length < 0) return HTTP_E_PROTO;
    if ((uint64_t)r->content_length > max_bytes || (uint64_t)r->content_length == SIZE_MAX) return HTTP_E_TOO_LARGE;
    size_t n = (size_t)r->content_length;
    char *raw = malloc(n + 1);
    if (raw == NULL) return HTTP_E_MEM;
    int e = recv_all(fd, raw, n, timeout_ms, cancel);
    if (e == 0 && !r->gzip) {
        raw[n] = '\0';
        out->data = raw;
        out->len = n;
        return 0;
    }
    if (e == 0) e = gunzip((const unsigned char *)raw, n, max_bytes, out);
    free(raw);
    return e;
}

int http_get(const srv_addr *a, const char *path, size_t max_bytes, uint32_t timeout_ms, const volatile bool *cancel, http_body *out)
{
    out->data = NULL;
    out->len = 0;
    out->status = 0;
    int fd = sk_connect(a->host, a->port, timeout_ms, cancel);
    if (fd < 0) return HTTP_E_CONNECT;
    hp_response r;
    int e = exchange(fd, a, path, "Accept-Encoding: gzip\r\n", timeout_ms, cancel, &r);
    if (e == 0) {
        out->status = r.status;
        e = read_body(fd, &r, max_bytes, timeout_ms, cancel, out);
    }
    sk_close(fd);
    return e;
}

void http_body_free(http_body *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
}

static int stream_request(http_stream *s, const char *range, hp_response *r)
{
    if (s->fd < 0) {
        int fd = sk_connect(s->addr.host, s->addr.port, s->timeout_ms, s->cancel);
        if (fd < 0) return HTTP_E_CONNECT;
        s->fd = fd;
    }
    return exchange(s->fd, &s->addr, s->path, range, s->timeout_ms, s->cancel, r);
}

static int check_range(const hp_response *r, uint64_t offset)
{
    if (r->status != 206) return HTTP_E_STATUS;
    if (r->gzip || r->total < 0 || r->range_start < 0 || (uint64_t)r->range_start != offset ||
        r->content_length != r->range_end - r->range_start + 1) {
        return HTTP_E_PROTO;
    }
    return 0;
}

int hs_open(http_stream *s, const srv_addr *a, const char *path, uint32_t timeout_ms, const volatile bool *cancel)
{
    s->addr = *a;
    s->path[0] = '\0';
    s->fd = -1;
    s->length = 0;
    s->remaining = 0;
    s->timeout_ms = timeout_ms;
    s->cancel = cancel;
    size_t n = strlen(path);
    if (n >= sizeof s->path) return HTTP_E_TOO_LARGE;
    memcpy(s->path, path, n + 1);

    hp_response r;
    int e = stream_request(s, "Range: bytes=0-0\r\n", &r);
    if (e == 0) e = check_range(&r, 0);
    if (e == 0 && r.range_end != 0) e = HTTP_E_PROTO;
    if (e == 0) {
        unsigned char first;
        s->remaining = 1;
        long got = hs_read(s, &first, 1);
        if (got < 0) e = (int)got;
    }
    if (e != 0) {
        hs_close(s);
        return e;
    }
    s->length = (uint64_t)r.total;
    if (!r.keep_alive) hs_close(s);
    return 0;
}

int hs_start(http_stream *s, uint64_t offset)
{
    if (s->remaining > 0) hs_close(s);
    char range[48];
    snprintf(range, sizeof range, "Range: bytes=%" PRIu64 "-\r\n", offset);
    bool reused = s->fd >= 0;
    hp_response r;
    int e = stream_request(s, range, &r);
    if (e == HTTP_E_CONNECT && reused) {
        hs_close(s);
        e = stream_request(s, range, &r);
    }
    if (e == 0) e = check_range(&r, offset);
    if (e != 0) {
        hs_close(s);
        return e;
    }
    s->remaining = (uint64_t)r.content_length;
    return 0;
}

long hs_read(http_stream *s, void *dst, size_t len)
{
    if (s->fd < 0) return HTTP_E_CONNECT;
    if (s->remaining == 0 || len == 0) return 0;
    size_t want = len < s->remaining ? len : (size_t)s->remaining;
    long n = sk_recv(s->fd, dst, want, s->timeout_ms, s->cancel);
    if (n == SK_E_TIMEOUT) return HTTP_E_TIMEOUT;
    if (n <= 0) {
        hs_close(s);
        return HTTP_E_CONNECT;
    }
    s->remaining -= (uint64_t)n;
    return n;
}

void hs_close(http_stream *s)
{
    sk_close(s->fd);
    s->fd = -1;
    s->remaining = 0;
}
