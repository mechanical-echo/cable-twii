#include "net/addr.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define SCHEME "http://"

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_unreserved(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '.' || c == '_' || c == '~' || c == '/';
}

int addr_parse(const char *typed, srv_addr *out)
{
    while (is_space(*typed)) typed++;
    size_t len = strlen(typed);
    while (len > 0 && is_space(typed[len - 1])) len--;
    if (len >= strlen(SCHEME) && strncasecmp(typed, SCHEME, strlen(SCHEME)) == 0) {
        typed += strlen(SCHEME);
        len -= strlen(SCHEME);
    }
    if (len > 0 && typed[len - 1] == '/') len--;

    size_t host_len = 0;
    while (host_len < len && typed[host_len] != ':') host_len++;
    if (host_len == 0 || host_len >= sizeof out->host) return -1;
    for (size_t i = 0; i < host_len; i++) {
        char c = typed[i];
        if (is_space(c) || c == '@' || c == '?' || c == '#' || c == '/') return -1;
    }

    unsigned long port = ADDR_DEFAULT_PORT;
    if (host_len < len) {
        const char *digits = typed + host_len + 1;
        size_t n = len - host_len - 1;
        if (n == 0 || n > 5) return -1;
        port = 0;
        for (size_t i = 0; i < n; i++) {
            if (digits[i] < '0' || digits[i] > '9') return -1;
            port = port * 10 + (unsigned long)(digits[i] - '0');
        }
        if (port < 1 || port > 65535) return -1;
    }

    memcpy(out->host, typed, host_len);
    out->host[host_len] = '\0';
    out->port = (uint16_t)port;
    return 0;
}

void addr_format(const srv_addr *a, char *out, size_t out_size)
{
    if (out_size == 0) return;
    snprintf(out, out_size, "%s:%u", a->host, (unsigned)a->port);
}

int url_encode_path(const char *rel, char *out, size_t out_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)rel; *p != '\0'; p++) {
        size_t need = is_unreserved(*p) ? 1 : 3;
        if (n + need + 1 > out_size) return -1;
        if (need == 1) {
            out[n++] = (char)*p;
        } else {
            out[n++] = '%';
            out[n++] = hex[*p >> 4];
            out[n++] = hex[*p & 15];
        }
    }
    if (n + 1 > out_size) return -1;
    out[n] = '\0';
    return 0;
}

int url_split(const char *url, srv_addr *addr, char *path, size_t path_size)
{
    size_t scheme = strlen(SCHEME);
    if (strncasecmp(url, SCHEME, scheme) != 0) return -1;
    const char *auth = url + scheme;
    const char *slash = strchr(auth, '/');
    size_t auth_len = slash != NULL ? (size_t)(slash - auth) : strlen(auth);
    char host_port[sizeof addr->host + 8];
    if (auth_len >= sizeof host_port) return -1;
    memcpy(host_port, auth, auth_len);
    host_port[auth_len] = '\0';
    if (addr_parse(host_port, addr) != 0) return -1;
    const char *rest = slash != NULL ? slash : "/";
    size_t rest_len = strlen(rest);
    if (rest_len + 1 > path_size) return -1;
    memcpy(path, rest, rest_len + 1);
    return 0;
}

void srv_root(const srv_addr *a, char *out, size_t out_size)
{
    if (out_size == 0) return;
    snprintf(out, out_size, SCHEME "%s:%u/wii/v1/files", a->host, (unsigned)a->port);
}
