#include <string.h>
#include "greatest/greatest.h"
#include "ui/utf8.h"

TEST utf8_ascii(void)
{
    uint32_t cp[8];
    size_t n = u8_decode("Hi!", 3, cp, 8);
    ASSERT_EQ(3u, n);
    ASSERT_EQ(0x48u, cp[0]);
    ASSERT_EQ(0x69u, cp[1]);
    ASSERT_EQ(0x21u, cp[2]);
    PASS();
}

TEST utf8_multibyte(void)
{
    const char *s = "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    uint32_t cp[8];
    size_t n = u8_decode(s, strlen(s), cp, 8);
    ASSERT_EQ(3u, n);
    ASSERT_EQ(0xE9u, cp[0]);
    ASSERT_EQ(0x20ACu, cp[1]);
    ASSERT_EQ(0x1F600u, cp[2]);
    PASS();
}

TEST utf8_ellipsis(void)
{
    const char *s = "Amaz\xE2\x80\xA6";
    uint32_t cp[8];
    size_t n = u8_decode(s, strlen(s), cp, 8);
    ASSERT_EQ(5u, n);
    ASSERT_EQ(0x7Au, cp[3]);
    ASSERT_EQ(0x2026u, cp[4]);
    PASS();
}

TEST utf8_invalid_bytes(void)
{
    const char *s = "a\xFF" "b\x80" "c";
    uint32_t cp[8];
    size_t n = u8_decode(s, strlen(s), cp, 8);
    ASSERT_EQ(5u, n);
    ASSERT_EQ(0x61u, cp[0]);
    ASSERT_EQ(U8_REPLACEMENT, cp[1]);
    ASSERT_EQ(0x62u, cp[2]);
    ASSERT_EQ(U8_REPLACEMENT, cp[3]);
    ASSERT_EQ(0x63u, cp[4]);
    PASS();
}

TEST utf8_truncated_sequence(void)
{
    const char *s = "\xE2\x82" "A";
    uint32_t cp[8];
    size_t n = u8_decode(s, strlen(s), cp, 8);
    ASSERT_EQ(2u, n);
    ASSERT_EQ(U8_REPLACEMENT, cp[0]);
    ASSERT_EQ(0x41u, cp[1]);
    n = u8_decode(s, 2, cp, 8);
    ASSERT_EQ(1u, n);
    ASSERT_EQ(U8_REPLACEMENT, cp[0]);
    PASS();
}

TEST utf8_overlong_and_surrogate(void)
{
    const char *s = "\xC0\xAF\xED\xA0\x80\xF4\x90\x80\x80";
    uint32_t cp[16];
    size_t n = u8_decode(s, strlen(s), cp, 16);
    ASSERT(n > 0);
    for (size_t i = 0; i < n; i++) ASSERT_EQ(U8_REPLACEMENT, cp[i]);
    PASS();
}

TEST utf8_respects_capacity(void)
{
    uint32_t cp[2];
    size_t n = u8_decode("abcdef", 6, cp, 2);
    ASSERT_EQ(2u, n);
    ASSERT_EQ(0x62u, cp[1]);
    PASS();
}

TEST utf8_prefix_bytes(void)
{
    const char *s = "Pok\xC3\xA9mon";
    ASSERT_EQ(3u, u8_prefix_bytes(s, 3));
    ASSERT_EQ(5u, u8_prefix_bytes(s, 4));
    ASSERT_EQ(strlen(s), u8_prefix_bytes(s, 100));
    PASS();
}

SUITE(utf8_suite)
{
    RUN_TEST(utf8_ascii);
    RUN_TEST(utf8_multibyte);
    RUN_TEST(utf8_ellipsis);
    RUN_TEST(utf8_invalid_bytes);
    RUN_TEST(utf8_truncated_sequence);
    RUN_TEST(utf8_overlong_and_surrogate);
    RUN_TEST(utf8_respects_capacity);
    RUN_TEST(utf8_prefix_bytes);
}
