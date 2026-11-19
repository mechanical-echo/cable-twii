#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "net/sock.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

#define POLL_SLICE_MS 50

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static bool would_block(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == EINPROGRESS;
}

static bool cancelled(const volatile bool *cancel)
{
    return cancel != NULL && *cancel;
}

static int wait_fd(int fd, short events, uint64_t start, uint32_t timeout_ms, const volatile bool *cancel)
{
    for (;;) {
        if (cancelled(cancel)) return SK_E_CANCEL;
        uint64_t spent = now_ms() - start;
        if (spent >= timeout_ms) return SK_E_TIMEOUT;
        uint64_t left = timeout_ms - spent;
        struct pollfd p = { fd, events, 0 };
        int r = poll(&p, 1, left < POLL_SLICE_MS ? (int)left : POLL_SLICE_MS);
        if (r > 0) return 0;
        if (r < 0 && errno != EINTR) return SK_E_FAIL;
    }
}

int sk_up(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return 0;
}

void sk_down(void)
{
}

int sk_connect(const char *host, uint16_t port, uint32_t timeout_ms, const volatile bool *cancel)
{
    if (cancelled(cancel)) return SK_E_CANCEL;
    uint64_t start = now_ms();
    char service[8];
    snprintf(service, sizeof service, "%u", (unsigned)port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *list = NULL;
    if (getaddrinfo(host, service, &hints, &list) != 0) return SK_E_FAIL;

    int fd = socket(list->ai_family, list->ai_socktype, list->ai_protocol);
    int flags = fd < 0 ? -1 : fcntl(fd, F_GETFL, 0);
    int r = flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ? SK_E_FAIL : 0;
#ifdef SO_NOSIGPIPE
    int on = 1;
    if (r == 0) setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif
    if (r == 0 && connect(fd, list->ai_addr, list->ai_addrlen) != 0) {
        r = would_block() ? wait_fd(fd, POLLOUT, start, timeout_ms, cancel) : SK_E_FAIL;
        int err = 0;
        socklen_t err_len = sizeof err;
        if (r == 0 && (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len) != 0 || err != 0)) r = SK_E_FAIL;
    }
    freeaddrinfo(list);
    if (r == 0) return fd;
    sk_close(fd);
    return r;
}

int sk_send_all(int fd, const void *buf, size_t len, uint32_t timeout_ms, const volatile bool *cancel)
{
    uint64_t start = now_ms();
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, SEND_FLAGS);
        if (n > 0) {
            p += n;
            len -= (size_t)n;
            continue;
        }
        if (n == 0 || !would_block()) return SK_E_FAIL;
        int w = wait_fd(fd, POLLOUT, start, timeout_ms, cancel);
        if (w != 0) return w;
    }
    return 0;
}

long sk_recv(int fd, void *buf, size_t len, uint32_t timeout_ms, const volatile bool *cancel)
{
    uint64_t start = now_ms();
    for (;;) {
        ssize_t n = recv(fd, buf, len, 0);
        if (n >= 0) return (long)n;
        if (!would_block()) return SK_E_FAIL;
        int w = wait_fd(fd, POLLIN, start, timeout_ms, cancel);
        if (w != 0) return w;
    }
}

void sk_close(int fd)
{
    if (fd >= 0) close(fd);
}
