#include "net/sock.h"

#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <network.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/mutex.h>

#define IO_MAX (16 * 1024)
#define POLL_SLICE_MS 50
#define IDLE_US 1000
#define CONNECT_STEP_US 5000
#define UP_STEP_US 10000

static volatile bool up_done;
static volatile s32 up_result;
static mutex_t lookup_lock = LWP_MUTEX_NULL;

static uint32_t since_ms(u64 start)
{
    return (uint32_t)ticks_to_millisecs(diff_ticks(start, gettime()));
}

static bool cancelled(const volatile bool *cancel)
{
    return cancel != NULL && *cancel;
}

static int wait_fd(int fd, u32 events, u64 start, uint32_t timeout_ms, const volatile bool *cancel)
{
    if (cancelled(cancel)) return SK_E_CANCEL;
    uint32_t spent = since_ms(start);
    if (spent >= timeout_ms) return SK_E_TIMEOUT;
    uint32_t left = timeout_ms - spent;
    struct pollsd p = { fd, events, 0 };
    if (net_poll(&p, 1, (s32)(left < POLL_SLICE_MS ? left : POLL_SLICE_MS)) <= 0 || (p.revents & events) == 0) {
        usleep(IDLE_US);
    }
    return 0;
}

static s32 up_cb(s32 result, void *usrdata)
{
    (void)usrdata;
    up_result = result;
    up_done = true;
    return 0;
}

int sk_up(uint32_t timeout_ms)
{
    if (lookup_lock == LWP_MUTEX_NULL) LWP_MutexInit(&lookup_lock, false);
    up_done = false;
    up_result = SK_E_FAIL;
    u64 start = gettime();
    if (net_init_async(up_cb, NULL) < 0) return SK_E_FAIL;
    while (!up_done) {
        if (net_get_status() == 0) return 0;
        if (since_ms(start) >= timeout_ms) {
            net_deinit();
            return SK_E_TIMEOUT;
        }
        usleep(UP_STEP_US);
    }
    return up_result < 0 ? SK_E_FAIL : 0;
}

void sk_down(void)
{
    net_deinit();
}

int sk_connect(const char *host, uint16_t port, uint32_t timeout_ms, const volatile bool *cancel)
{
    if (cancelled(cancel)) return SK_E_CANCEL;
    u64 start = gettime();
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_len = sizeof sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    if (!inet_aton(host, &sa.sin_addr)) {
        LWP_MutexLock(lookup_lock);
        struct hostent *h = net_gethostbyname(host);
        bool found = h != NULL && h->h_addr_list != NULL && h->h_addr_list[0] != NULL;
        if (found) memcpy(&sa.sin_addr, h->h_addr_list[0], sizeof sa.sin_addr);
        LWP_MutexUnlock(lookup_lock);
        if (!found) return SK_E_FAIL;
    }

    s32 fd = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) return SK_E_FAIL;
    u32 on = 1;
    int r = net_ioctl(fd, FIONBIO, &on) < 0 ? SK_E_FAIL : 0;
    while (r == 0) {
        s32 c = net_connect(fd, (struct sockaddr *)&sa, sizeof sa);
        if (c == 0 || c == -EISCONN) return fd;
        if (c != -EINPROGRESS && c != -EALREADY) {
            r = SK_E_FAIL;
        } else if (cancelled(cancel)) {
            r = SK_E_CANCEL;
        } else if (since_ms(start) >= timeout_ms) {
            r = SK_E_TIMEOUT;
        } else {
            usleep(CONNECT_STEP_US);
        }
    }
    net_close(fd);
    return r;
}

int sk_send_all(int fd, const void *buf, size_t len, uint32_t timeout_ms, const volatile bool *cancel)
{
    u64 start = gettime();
    const char *p = buf;
    while (len > 0) {
        s32 n = net_send(fd, p, (s32)(len < IO_MAX ? len : IO_MAX), 0);
        if (n > 0) {
            p += n;
            len -= (size_t)n;
            continue;
        }
        if (n != -EAGAIN) return SK_E_FAIL;
        int w = wait_fd(fd, POLLOUT, start, timeout_ms, cancel);
        if (w != 0) return w;
    }
    return 0;
}

long sk_recv(int fd, void *buf, size_t len, uint32_t timeout_ms, const volatile bool *cancel)
{
    u64 start = gettime();
    s32 want = (s32)(len < IO_MAX ? len : IO_MAX);
    for (;;) {
        s32 n = net_recv(fd, buf, want, 0);
        if (n >= 0) return n;
        if (n != -EAGAIN) return SK_E_FAIL;
        int w = wait_fd(fd, POLLIN, start, timeout_ms, cancel);
        if (w != 0) return w;
    }
}

void sk_close(int fd)
{
    if (fd >= 0) net_close(fd);
}
