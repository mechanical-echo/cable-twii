#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "greatest/greatest.h"
#include "net/http.h"
#include "net/sock.h"
#include "testserver.h"

#define LONG_WAIT_MS 5000u
#define SHORT_WAIT_MS 100u
#define CANCEL_AFTER_MS 100
#define CANCEL_MARGIN_MS 400

static volatile bool cancel;
static pthread_t canceller;
static bool canceller_on;
static int fd = -1;
static http_body body;
static http_stream stream = { .fd = -1 };

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void *cancel_later(void *arg)
{
    (void)arg;
    struct timespec ts = { 0, CANCEL_AFTER_MS * 1000000L };
    nanosleep(&ts, NULL);
    cancel = true;
    return NULL;
}

static bool cancel_soon(void)
{
    cancel = false;
    canceller_on = pthread_create(&canceller, NULL, cancel_later, NULL) == 0;
    return canceller_on;
}

static void teardown(void *udata)
{
    (void)udata;
    if (canceller_on) pthread_join(canceller, NULL);
    canceller_on = false;
    cancel = false;
    sk_close(fd);
    fd = -1;
    http_body_free(&body);
    hs_close(&stream);
    ts_stop();
}

static srv_addr silent_server(void)
{
    srv_addr a = { "127.0.0.1", ts_start(NULL, 0) };
    return a;
}

TEST recv_returns_soon_after_cancel(void)
{
    srv_addr a = silent_server();
    ASSERT(a.port != 0);
    fd = sk_connect(a.host, a.port, LONG_WAIT_MS, &cancel);
    ASSERT(fd >= 0);
    ASSERT(cancel_soon());
    char buf[16];
    uint64_t start = now_ms();
    ASSERT_EQ(SK_E_CANCEL, sk_recv(fd, buf, sizeof buf, LONG_WAIT_MS, &cancel));
    ASSERT(now_ms() - start < CANCEL_AFTER_MS + CANCEL_MARGIN_MS);
    PASS();
}

TEST connect_after_cancel_returns_at_once(void)
{
    srv_addr a = silent_server();
    ASSERT(a.port != 0);
    cancel = true;
    fd = sk_connect(a.host, a.port, LONG_WAIT_MS, &cancel);
    ASSERT_EQ(SK_E_CANCEL, fd);
    PASS();
}

TEST recv_without_cancel_waits_for_the_timeout(void)
{
    srv_addr a = silent_server();
    ASSERT(a.port != 0);
    fd = sk_connect(a.host, a.port, LONG_WAIT_MS, NULL);
    ASSERT(fd >= 0);
    char buf[16];
    uint64_t start = now_ms();
    ASSERT_EQ(SK_E_TIMEOUT, sk_recv(fd, buf, sizeof buf, SHORT_WAIT_MS, NULL));
    ASSERT(now_ms() - start >= SHORT_WAIT_MS);
    PASS();
}

TEST get_returns_soon_after_cancel(void)
{
    srv_addr a = silent_server();
    ASSERT(a.port != 0);
    ASSERT(cancel_soon());
    uint64_t start = now_ms();
    ASSERT_EQ(HTTP_E_CONNECT, http_get(&a, "/wii/v1/info", 4096, LONG_WAIT_MS, &cancel, &body));
    ASSERT(now_ms() - start < CANCEL_AFTER_MS + CANCEL_MARGIN_MS);
    ASSERT_EQ(NULL, body.data);
    PASS();
}

TEST stream_returns_soon_after_cancel(void)
{
    srv_addr a = silent_server();
    ASSERT(a.port != 0);
    ASSERT(cancel_soon());
    uint64_t start = now_ms();
    ASSERT_EQ(HTTP_E_CONNECT, hs_open(&stream, &a, "/wii/v1/files/a.mpg", LONG_WAIT_MS, &cancel));
    ASSERT(now_ms() - start < CANCEL_AFTER_MS + CANCEL_MARGIN_MS);
    ASSERT_EQ(-1, stream.fd);
    PASS();
}

SUITE(sock_suite)
{
    SET_TEARDOWN(teardown, NULL);
    RUN_TEST(recv_returns_soon_after_cancel);
    RUN_TEST(connect_after_cancel_returns_at_once);
    RUN_TEST(recv_without_cancel_waits_for_the_timeout);
    RUN_TEST(get_returns_soon_after_cancel);
    RUN_TEST(stream_returns_soon_after_cancel);
    SET_TEARDOWN(NULL, NULL);
}
