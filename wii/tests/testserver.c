#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "testserver.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define TS_MAX 8
#define TS_HEAD_MAX 4096
#define TS_TICK_MS 10

#ifdef MSG_NOSIGNAL
#define TS_SEND_FLAGS MSG_NOSIGNAL
#else
#define TS_SEND_FLAGS 0
#endif

static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool stopping;
static bool running;
static int listen_fd = -1;
static const ts_reply *replies;
static size_t reply_count;
static char heads[TS_MAX][TS_HEAD_MAX];
static size_t conns[TS_MAX];

static void no_sigpipe(int fd)
{
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#else
    (void)fd;
#endif
}

static bool wait_readable(int fd)
{
    struct pollfd p = { fd, POLLIN, 0 };
    while (!atomic_load(&stopping)) {
        int r = poll(&p, 1, TS_TICK_MS);
        if (r > 0) return true;
        if (r < 0 && errno != EINTR) return false;
    }
    return false;
}

static bool read_head(int fd, char *out)
{
    size_t n = 0;
    while (n + 1 < TS_HEAD_MAX) {
        if (!wait_readable(fd) || recv(fd, out + n, 1, 0) != 1) return false;
        n++;
        out[n] = '\0';
        if (n >= 4 && memcmp(out + n - 4, "\r\n\r\n", 4) == 0) return true;
    }
    return false;
}

static bool send_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t r = send(fd, p, len, TS_SEND_FLAGS);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        p += r;
        len -= (size_t)r;
    }
    return true;
}

static void *serve(void *arg)
{
    (void)arg;
    char head[TS_HEAD_MAX];
    size_t next = 0;
    size_t conn = 0;
    while (next < reply_count && wait_readable(listen_fd)) {
        int c = accept(listen_fd, NULL, NULL);
        if (c < 0) continue;
        no_sigpipe(c);
        while (next < reply_count && read_head(c, head)) {
            const ts_reply *r = &replies[next];
            pthread_mutex_lock(&lock);
            memcpy(heads[next], head, sizeof head);
            conns[next] = conn;
            pthread_mutex_unlock(&lock);
            next++;
            if (!send_all(c, r->head, strlen(r->head)) || !send_all(c, r->body, r->body_len) || r->close_after) break;
        }
        close(c);
        conn++;
    }
    return NULL;
}

uint16_t ts_start(const ts_reply *list, size_t n)
{
    if (running || n > TS_MAX) return 0;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t sa_len = sizeof sa;
    if (bind(fd, (struct sockaddr *)&sa, sizeof sa) != 0 || listen(fd, 4) != 0 ||
        getsockname(fd, (struct sockaddr *)&sa, &sa_len) != 0) {
        close(fd);
        return 0;
    }
    pthread_mutex_lock(&lock);
    memset(heads, 0, sizeof heads);
    memset(conns, 0, sizeof conns);
    pthread_mutex_unlock(&lock);
    replies = list;
    reply_count = n;
    listen_fd = fd;
    atomic_store(&stopping, false);
    if (pthread_create(&thread, NULL, serve, NULL) != 0) {
        close(fd);
        listen_fd = -1;
        return 0;
    }
    running = true;
    return ntohs(sa.sin_port);
}

const char *ts_request(size_t i)
{
    if (i >= TS_MAX) return "";
    pthread_mutex_lock(&lock);
    const char *head = heads[i];
    pthread_mutex_unlock(&lock);
    return head;
}

size_t ts_conn(size_t i)
{
    if (i >= TS_MAX) return 0;
    pthread_mutex_lock(&lock);
    size_t conn = conns[i];
    pthread_mutex_unlock(&lock);
    return conn;
}

void ts_stop(void)
{
    if (!running) return;
    atomic_store(&stopping, true);
    pthread_join(thread, NULL);
    close(listen_fd);
    listen_fd = -1;
    running = false;
}
