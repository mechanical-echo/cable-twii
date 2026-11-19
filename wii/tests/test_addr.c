#include <stdio.h>
#include <string.h>
#include "greatest/greatest.h"
#include "net/addr.h"

TEST addr_accepts_loose_forms(void)
{
    srv_addr a;
    ASSERT_EQ(0, addr_parse("http://192.168.1.20:4747/", &a));
    ASSERT_STR_EQ("192.168.1.20", a.host);
    ASSERT_EQ(4747, a.port);
    ASSERT_EQ(0, addr_parse(" 192.168.1.20 ", &a));
    ASSERT_STR_EQ("192.168.1.20", a.host);
    ASSERT_EQ(ADDR_DEFAULT_PORT, a.port);
    ASSERT_EQ(0, addr_parse("PC-NAME:8080", &a));
    ASSERT_STR_EQ("PC-NAME", a.host);
    ASSERT_EQ(8080, a.port);
    ASSERT_EQ(0, addr_parse("HTTP://box", &a));
    ASSERT_STR_EQ("box", a.host);
    PASS();
}

TEST addr_refuses_https_and_bad_port(void)
{
    srv_addr a;
    ASSERT_EQ(-1, addr_parse("https://x", &a));
    ASSERT_EQ(-1, addr_parse("x:0", &a));
    ASSERT_EQ(-1, addr_parse("x:99999", &a));
    ASSERT_EQ(-1, addr_parse("x:", &a));
    ASSERT_EQ(-1, addr_parse("x:80a", &a));
    ASSERT_EQ(-1, addr_parse("", &a));
    ASSERT_EQ(-1, addr_parse("   ", &a));
    ASSERT_EQ(-1, addr_parse("a b", &a));
    ASSERT_EQ(-1, addr_parse("x/y", &a));
    ASSERT_EQ(-1, addr_parse("http://x//", &a));
    ASSERT_EQ(-1, addr_parse("u@x", &a));
    ASSERT_EQ(-1, addr_parse("x?y", &a));
    ASSERT_EQ(-1, addr_parse("x#y", &a));
    ASSERT_EQ(-1, addr_parse(":80", &a));
    PASS();
}

TEST addr_refuses_long_host(void)
{
    srv_addr a;
    char host[80];
    memset(host, 'a', sizeof host);
    host[63] = '\0';
    ASSERT_EQ(0, addr_parse(host, &a));
    host[64] = '\0';
    host[63] = 'a';
    ASSERT_EQ(-1, addr_parse(host, &a));
    PASS();
}

TEST addr_formats_host_and_port(void)
{
    srv_addr a = { "box", 8080 };
    char buf[32];
    addr_format(&a, buf, sizeof buf);
    ASSERT_STR_EQ("box:8080", buf);
    addr_format(&a, buf, 5);
    ASSERT_STR_EQ("box:", buf);
    PASS();
}

TEST url_encodes_unicode_path(void)
{
    char out[256];
    ASSERT_EQ(0, url_encode_path("Cartoon Network/Pok\xC3\xA9mon \xE2\x9C\xA8/S01E02 - Caf\xC3\xA9 \xC3\xBC" "ber alles.mpg", out, sizeof out));
    ASSERT_STR_EQ("Cartoon%20Network/Pok%C3%A9mon%20%E2%9C%A8/S01E02%20-%20Caf%C3%A9%20%C3%BCber%20alles.mpg", out);
    PASS();
}

TEST url_encode_too_small_fails(void)
{
    char out[4];
    ASSERT_EQ(-1, url_encode_path("a b", out, sizeof out));
    ASSERT_EQ(0, url_encode_path("a b", out, 6));
    PASS();
}

TEST url_split_round_trip(void)
{
    srv_addr a = { "192.168.1.20", 4747 };
    srv_addr b;
    char root[128];
    char enc[64];
    char url[256];
    char path[128];
    srv_root(&a, root, sizeof root);
    ASSERT_STR_EQ("http://192.168.1.20:4747/wii/v1/files", root);
    ASSERT_EQ(0, url_encode_path("/a b.mpg", enc, sizeof enc));
    snprintf(url, sizeof url, "%s%s", root, enc);
    ASSERT_EQ(0, url_split(url, &b, path, sizeof path));
    ASSERT_STR_EQ(a.host, b.host);
    ASSERT_EQ(a.port, b.port);
    ASSERT_STR_EQ("/wii/v1/files/a%20b.mpg", path);
    PASS();
}

TEST url_split_refuses_bad_input(void)
{
    srv_addr a;
    char path[8];
    ASSERT_EQ(-1, url_split("https://x/y", &a, path, sizeof path));
    ASSERT_EQ(-1, url_split("x/y", &a, path, sizeof path));
    ASSERT_EQ(-1, url_split("http://x:4747/toolongpath", &a, path, sizeof path));
    ASSERT_EQ(0, url_split("http://x", &a, path, sizeof path));
    ASSERT_STR_EQ("/", path);
    ASSERT_EQ(ADDR_DEFAULT_PORT, a.port);
    PASS();
}

TEST long_accented_path_encodes(void)
{
    srv_addr a = { "server-name-long-enough-to-fill-the-whole-host-field-0123456789", 65535 };
    ASSERT_EQ(sizeof a.host - 1, strlen(a.host));
    static char rel[ADDR_REL_MAX + 1];
    size_t n = 0;
    while (n + 3 < ADDR_REL_MAX) {
        rel[n++] = '\xC3';
        rel[n++] = '\xA9';
    }
    memcpy(rel + n, "\xE2\x9C\xA8", 4);
    ASSERT_EQ(ADDR_REL_MAX, strlen(rel));
    char root[128];
    srv_root(&a, root, sizeof root);
    static char url[ADDR_URL_BYTES];
    int len = snprintf(url, sizeof url, "%s/%s", root, rel);
    ASSERT(len > 0 && (size_t)len < sizeof url);
    srv_addr b;
    static char raw[ADDR_URL_BYTES];
    static char path[ADDR_PATH_BYTES];
    ASSERT_EQ(0, url_split(url, &b, raw, sizeof raw));
    ASSERT_EQ(0, url_encode_path(raw, path, sizeof path));
    ASSERT_EQ(strlen("/wii/v1/files/") + 3 * ADDR_REL_MAX, strlen(path));
    ASSERT_EQ(0, strncmp(path, "/wii/v1/files/%C3%A9%C3%A9", 26));
    ASSERT_STR_EQ("%C3%A9%E2%9C%A8", path + strlen(path) - 15);
    ASSERT_STR_EQ(a.host, b.host);
    PASS();
}

SUITE(addr_suite)
{
    RUN_TEST(addr_accepts_loose_forms);
    RUN_TEST(addr_refuses_https_and_bad_port);
    RUN_TEST(addr_refuses_long_host);
    RUN_TEST(addr_formats_host_and_port);
    RUN_TEST(url_encodes_unicode_path);
    RUN_TEST(url_encode_too_small_fails);
    RUN_TEST(url_split_round_trip);
    RUN_TEST(url_split_refuses_bad_input);
    RUN_TEST(long_accented_path_encodes);
}
