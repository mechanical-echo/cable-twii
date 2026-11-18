#include <string.h>
#include "greatest/greatest.h"
#include "net/httpparse.h"

static long parse(const char *head, hp_response *out)
{
    return hp_parse_head(head, strlen(head), out);
}

TEST head_206_range(void)
{
    const char *head = "HTTP/1.1 206 Partial Content\r\n"
                       "content-type: video/mpeg\r\n"
                       "Content-Range: bytes 0-9/1000\r\n"
                       "CONTENT-LENGTH:   10 \r\n"
                       "\r\n"
                       "0123456789";
    hp_response r;
    ASSERT_EQ((long)strlen(head) - 10, parse(head, &r));
    ASSERT_EQ(206, r.status);
    ASSERT_EQ(0, r.range_start);
    ASSERT_EQ(9, r.range_end);
    ASSERT_EQ(1000, r.total);
    ASSERT_EQ(10, r.content_length);
    ASSERT_FALSE(r.gzip);
    ASSERT(r.keep_alive);

    ASSERT(parse("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */1000\r\nContent-Length: 0\r\n\r\n", &r) > 0);
    ASSERT_EQ(416, r.status);
    ASSERT_EQ(-1, r.range_start);
    ASSERT_EQ(-1, r.range_end);
    ASSERT_EQ(1000, r.total);
    ASSERT_EQ(0, r.content_length);
    PASS();
}

TEST head_incomplete_returns_0(void)
{
    hp_response r;
    ASSERT_EQ(0, parse("", &r));
    ASSERT_EQ(0, parse("HTT", &r));
    ASSERT_EQ(0, parse("HTTP/1.1 200 OK\r\n", &r));
    ASSERT_EQ(0, parse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r", &r));
    PASS();
}

TEST head_gzip_and_close(void)
{
    hp_response r;
    ASSERT(parse("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nConnection: close\r\nContent-Length: 20\r\n\r\n", &r) > 0);
    ASSERT_EQ(200, r.status);
    ASSERT(r.gzip);
    ASSERT_FALSE(r.keep_alive);
    ASSERT_EQ(20, r.content_length);
    ASSERT_EQ(-1, r.range_start);
    ASSERT_EQ(-1, r.range_end);
    ASSERT_EQ(-1, r.total);

    ASSERT(parse("HTTP/1.1 200 OK\r\nconnection: Keep-Alive\r\n\r\n", &r) > 0);
    ASSERT(r.keep_alive);
    ASSERT_FALSE(r.gzip);
    ASSERT_EQ(-1, r.content_length);
    ASSERT(parse("HTTP/1.0 200 OK\r\n\r\n", &r) > 0);
    ASSERT_FALSE(r.keep_alive);
    PASS();
}

TEST head_malformed_returns_minus_1(void)
{
    hp_response r;
    ASSERT_EQ(-1, parse("SSH-2.0-OpenSSH_9.6\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/2 200\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 2x0 OK\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 200 OK\r\nno colon here\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 200 OK\r\nContent-Length: 12x\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 206 OK\r\nContent-Range: bytes 9-0/1000\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 206 OK\r\nContent-Range: bytes 0-1000/1000\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 206 OK\r\nContent-Range: items 0-9/1000\r\n\r\n", &r));
    ASSERT_EQ(-1, parse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", &r));
    PASS();
}

SUITE(httpparse_suite)
{
    RUN_TEST(head_206_range);
    RUN_TEST(head_incomplete_returns_0);
    RUN_TEST(head_gzip_and_close);
    RUN_TEST(head_malformed_returns_minus_1);
}
