#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "greatest/greatest.h"
#include "net/http.h"
#include "testserver.h"

#define WAIT_MS 2000
#define FILE_LEN 1000
#define READ_STEP 64
#define OPEN_HEAD "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 0-0/1000\r\nContent-Length: 1\r\n\r\n"

static unsigned char file[FILE_LEN];
static http_body body;
static http_stream stream = { .fd = -1 };

static void teardown(void *udata)
{
    (void)udata;
    http_body_free(&body);
    hs_close(&stream);
    ts_stop();
}

static void fill_file(void)
{
    for (size_t i = 0; i < FILE_LEN; i++) file[i] = (unsigned char)(i * 31 + 7);
}

static srv_addr local(uint16_t port)
{
    srv_addr a = { "127.0.0.1", port };
    return a;
}

static size_t gzip_text(const char *text, unsigned char *out, size_t out_size)
{
    z_stream z;
    memset(&z, 0, sizeof z);
    if (deflateInit2(&z, Z_BEST_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) return 0;
    z.next_in = (Bytef *)text;
    z.avail_in = (uInt)strlen(text);
    z.next_out = out;
    z.avail_out = (uInt)out_size;
    int r = deflate(&z, Z_FINISH);
    size_t n = z.total_out;
    deflateEnd(&z);
    return r == Z_STREAM_END ? n : 0;
}

static void range_head(char *out, size_t out_size, size_t start)
{
    snprintf(out, out_size, "HTTP/1.1 206 Partial Content\r\nContent-Type: video/mpeg\r\nContent-Range: bytes %zu-%d/%d\r\nContent-Length: %zu\r\n\r\n",
             start, FILE_LEN - 1, FILE_LEN, FILE_LEN - start);
}

static long read_up_to(http_stream *s, unsigned char *dst, size_t want)
{
    size_t got = 0;
    while (got < want) {
        size_t step = want - got < READ_STEP ? want - got : READ_STEP;
        long n = hs_read(s, dst + got, step);
        if (n < 0) return n;
        if (n == 0) break;
        got += (size_t)n;
    }
    return (long)got;
}

TEST get_inflates_gzip(void)
{
    unsigned char gz[128];
    size_t gz_len = gzip_text("version = 1\n", gz, sizeof gz);
    ASSERT(gz_len > 0);
    char head[160];
    snprintf(head, sizeof head, "HTTP/1.1 200 OK\r\nContent-Type: application/toml\r\nContent-Encoding: gzip\r\nContent-Length: %zu\r\n\r\n", gz_len);
    ts_reply replies[] = { { head, gz, gz_len, false } };
    uint16_t port = ts_start(replies, 1);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(0, http_get(&a, "/wii/v1/files/library.toml", 4096, WAIT_MS, &body));
    ASSERT_EQ(200, body.status);
    ASSERT_EQ(strlen("version = 1\n"), body.len);
    ASSERT_STR_EQ("version = 1\n", body.data);
    ASSERT_EQ(ts_request(0), strstr(ts_request(0), "GET /wii/v1/files/library.toml HTTP/1.1\r\n"));
    ASSERT(strstr(ts_request(0), "\r\nAccept-Encoding: gzip\r\n") != NULL);
    ASSERT(strstr(ts_request(0), "\r\nConnection: close\r\n") == NULL);
    http_body_free(&body);
    ASSERT_EQ(NULL, body.data);
    PASS();
}

TEST get_404_is_status_error(void)
{
    ts_reply replies[] = { { "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\n", "Not found", 9, false } };
    uint16_t port = ts_start(replies, 1);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(HTTP_E_STATUS, http_get(&a, "/wii/v1/files/none.toml", 4096, WAIT_MS, &body));
    ASSERT_EQ(404, body.status);
    ASSERT_EQ(NULL, body.data);
    PASS();
}

TEST get_too_large(void)
{
    unsigned char gz[128];
    char text[201];
    memset(text, 'a', 200);
    text[200] = '\0';
    size_t gz_len = gzip_text(text, gz, sizeof gz);
    ASSERT(gz_len > 0 && gz_len < 100);
    char head[160];
    snprintf(head, sizeof head, "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: %zu\r\n\r\n", gz_len);
    ts_reply replies[] = {
        { "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n", "0123456789", 10, false },
        { head, gz, gz_len, false },
    };
    uint16_t port = ts_start(replies, 2);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(HTTP_E_TOO_LARGE, http_get(&a, "/wii/v1/files/a.toml", 4, WAIT_MS, &body));
    ASSERT_EQ(NULL, body.data);
    ASSERT_EQ(HTTP_E_TOO_LARGE, http_get(&a, "/wii/v1/files/b.toml", 100, WAIT_MS, &body));
    ASSERT_EQ(NULL, body.data);
    PASS();
}

TEST stream_reads_open_ended_range(void)
{
    char head[192];
    range_head(head, sizeof head, 100);
    ts_reply replies[] = {
        { OPEN_HEAD, file, 1, false },
        { head, file + 100, FILE_LEN - 100, false },
    };
    uint16_t port = ts_start(replies, 2);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(0, hs_open(&stream, &a, "/wii/v1/files/Cartoons/a%20b.mpg", WAIT_MS));
    ASSERT_EQ(FILE_LEN, stream.length);
    ASSERT_EQ(0, stream.remaining);
    ASSERT_EQ(ts_request(0), strstr(ts_request(0), "GET /wii/v1/files/Cartoons/a%20b.mpg HTTP/1.1\r\n"));
    ASSERT(strstr(ts_request(0), "\r\nRange: bytes=0-0\r\n") != NULL);

    ASSERT_EQ(0, hs_start(&stream, 100));
    ASSERT_EQ(FILE_LEN - 100, stream.remaining);
    unsigned char got[FILE_LEN];
    ASSERT_EQ(FILE_LEN - 100, read_up_to(&stream, got, sizeof got));
    ASSERT_MEM_EQ(file + 100, got, FILE_LEN - 100);
    ASSERT_EQ(0, hs_read(&stream, got, sizeof got));
    ASSERT(strstr(ts_request(1), "\r\nRange: bytes=100-\r\n") != NULL);
    ASSERT_EQ(0, ts_conn(1));
    PASS();
}

TEST stream_restart_drops_unfinished_body(void)
{
    char head100[192];
    char head500[192];
    range_head(head100, sizeof head100, 100);
    range_head(head500, sizeof head500, 500);
    ts_reply replies[] = {
        { OPEN_HEAD, file, 1, false },
        { head100, file + 100, FILE_LEN - 100, false },
        { head500, file + 500, FILE_LEN - 500, false },
    };
    uint16_t port = ts_start(replies, 3);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(0, hs_open(&stream, &a, "/wii/v1/files/a.mpg", WAIT_MS));
    ASSERT_EQ(0, hs_start(&stream, 100));
    unsigned char got[FILE_LEN];
    ASSERT_EQ(10, read_up_to(&stream, got, 10));
    ASSERT_MEM_EQ(file + 100, got, 10);

    ASSERT_EQ(0, hs_start(&stream, 500));
    ASSERT_EQ(FILE_LEN - 500, stream.remaining);
    ASSERT_EQ(FILE_LEN - 500, read_up_to(&stream, got, sizeof got));
    ASSERT_MEM_EQ(file + 500, got, FILE_LEN - 500);
    ASSERT_EQ(0, hs_read(&stream, got, sizeof got));
    ASSERT(strstr(ts_request(2), "\r\nRange: bytes=500-\r\n") != NULL);
    ASSERT_EQ(0, ts_conn(1));
    ASSERT_EQ(1, ts_conn(2));
    PASS();
}

TEST stream_reconnects_after_server_close(void)
{
    char head[192];
    range_head(head, sizeof head, 0);
    ts_reply replies[] = {
        { OPEN_HEAD, file, 1, true },
        { head, file, FILE_LEN, false },
    };
    uint16_t port = ts_start(replies, 2);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(0, hs_open(&stream, &a, "/wii/v1/files/a.mpg", WAIT_MS));
    ASSERT_EQ(0, hs_start(&stream, 0));
    unsigned char got[FILE_LEN];
    ASSERT_EQ(FILE_LEN, read_up_to(&stream, got, sizeof got));
    ASSERT_MEM_EQ(file, got, FILE_LEN);
    ASSERT_EQ(0, hs_read(&stream, got, sizeof got));
    ASSERT(strstr(ts_request(1), "\r\nRange: bytes=0-\r\n") != NULL);
    ASSERT_EQ(1, ts_conn(1));
    PASS();
}

TEST stream_non_206_is_status_error(void)
{
    char head[96];
    snprintf(head, sizeof head, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", FILE_LEN);
    ts_reply replies[] = { { head, file, FILE_LEN, false } };
    uint16_t port = ts_start(replies, 1);
    ASSERT(port != 0);
    srv_addr a = local(port);
    ASSERT_EQ(HTTP_E_STATUS, hs_open(&stream, &a, "/wii/v1/files/a.mpg", WAIT_MS));
    ASSERT_EQ(-1, stream.fd);
    PASS();
}

TEST connect_refused_is_connect_error(void)
{
    uint16_t port = ts_start(NULL, 0);
    ASSERT(port != 0);
    ts_stop();
    srv_addr a = local(port);
    ASSERT_EQ(HTTP_E_CONNECT, http_get(&a, "/wii/v1/info", 4096, WAIT_MS, &body));
    ASSERT_EQ(0, body.status);
    ASSERT_EQ(NULL, body.data);
    ASSERT_EQ(HTTP_E_CONNECT, hs_open(&stream, &a, "/wii/v1/files/a.mpg", WAIT_MS));
    ASSERT_EQ(-1, stream.fd);
    PASS();
}

SUITE(http_suite)
{
    fill_file();
    SET_TEARDOWN(teardown, NULL);
    RUN_TEST(get_inflates_gzip);
    RUN_TEST(get_404_is_status_error);
    RUN_TEST(get_too_large);
    RUN_TEST(stream_reads_open_ended_range);
    RUN_TEST(stream_restart_drops_unfinished_body);
    RUN_TEST(stream_reconnects_after_server_close);
    RUN_TEST(stream_non_206_is_status_error);
    RUN_TEST(connect_refused_is_connect_error);
    SET_TEARDOWN(NULL, NULL);
}
