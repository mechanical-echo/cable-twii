#include <string.h>
#include "greatest/greatest.h"
#include "ui/textfit.h"

static float fixed_width(const char *s, size_t len, void *ctx)
{
    (void)s;
    (void)ctx;
    return (float)len * 10.0f;
}

TEST text_fits_whole(void)
{
    char o[64];
    tf_fit("Gumball", 100, fixed_width, NULL, o, sizeof o);
    ASSERT_STR_EQ("Gumball", o);
    PASS();
}

TEST text_fit_ellipsizes(void)
{
    char o[64];
    tf_fit("The Amazing World of Gumball", 100, fixed_width, NULL, o, sizeof o);
    ASSERT_STR_EQ("The Amaz\xE2\x80\xA6", o);
    PASS();
}

TEST text_fit_utf8_boundary(void)
{
    char o[64];
    tf_fit("Pok\xC3\xA9mon Indigo League", 60, fixed_width, NULL, o, sizeof o);
    ASSERT(o[strlen(o) - 4] != (char)0xC3);
    PASS();
}

TEST wrap_words(void)
{
    tf_line lines[4];
    size_t n = tf_wrap("The quick brown fox", 100, 4, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("The quick", lines[0].text);
    ASSERT_STR_EQ("brown fox", lines[1].text);
    PASS();
}

TEST wrap_hard_breaks(void)
{
    tf_line lines[4];
    size_t n = tf_wrap("One\n\nTwo", 100, 4, fixed_width, NULL, lines);
    ASSERT_EQ(3u, n);
    ASSERT_STR_EQ("One", lines[0].text);
    ASSERT_STR_EQ("", lines[1].text);
    ASSERT_STR_EQ("Two", lines[2].text);
    PASS();
}

TEST wrap_no_blank_line_after_trailing_space(void)
{
    tf_line lines[4];
    size_t n = tf_wrap("aaa bbb \nccc", 70, 4, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("aaa bbb", lines[0].text);
    ASSERT_STR_EQ("ccc", lines[1].text);
    PASS();
}

TEST wrap_breaks_long_words(void)
{
    tf_line lines[4];
    size_t n = tf_wrap("Supercalifragilistic", 100, 4, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("Supercalif", lines[0].text);
    ASSERT_STR_EQ("ragilistic", lines[1].text);
    PASS();
}

TEST wrap_ellipsizes_last_line(void)
{
    tf_line lines[2];
    size_t n = tf_wrap("aaa bbb ccc ddd eee", 70, 2, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("aaa bbb", lines[0].text);
    ASSERT_STR_EQ("ccc dd\xE2\x80\xA6", lines[1].text);
    PASS();
}

TEST wrap_ellipsizes_before_hidden_paragraph(void)
{
    tf_line lines[2];
    size_t n = tf_wrap("One\nTwo\nThree", 100, 2, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("One", lines[0].text);
    ASSERT_STR_EQ("Two\xE2\x80\xA6", lines[1].text);
    PASS();
}

TEST wrap_exact_fit_has_no_ellipsis(void)
{
    tf_line lines[2];
    size_t n = tf_wrap("aaa bbb ccc ddd", 70, 2, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("ccc ddd", lines[1].text);
    PASS();
}

TEST wrap_lines_fit_width(void)
{
    const char *body = "Plug in the drive prepared with cable-twii Studio, or put a cable-twii folder on the SD card.";
    tf_line lines[8];
    size_t n = tf_wrap(body, 350, 8, fixed_width, NULL, lines);
    ASSERT_EQ(3u, n);
    for (size_t i = 0; i < n; i++) ASSERT(strlen(lines[i].text) * 10 <= 350);
    ASSERT_STR_EQ("Plug in the drive prepared with", lines[0].text);
    PASS();
}

TEST wrap_keeps_utf8_whole(void)
{
    tf_line lines[4];
    size_t n = tf_wrap("Pok\xC3\xA9mon", 40, 4, fixed_width, NULL, lines);
    ASSERT_EQ(2u, n);
    ASSERT_STR_EQ("Pok\xC3\xA9", lines[0].text);
    ASSERT_STR_EQ("mon", lines[1].text);
    PASS();
}

TEST wrap_caps_line_bytes(void)
{
    char big[600];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    tf_line lines[4];
    size_t n = tf_wrap(big, 1e9f, 4, fixed_width, NULL, lines);
    ASSERT(n >= 2u);
    ASSERT(strlen(lines[0].text) < TF_LINE_BYTES);
    PASS();
}

SUITE(textfit_suite)
{
    RUN_TEST(text_fits_whole);
    RUN_TEST(text_fit_ellipsizes);
    RUN_TEST(text_fit_utf8_boundary);
    RUN_TEST(wrap_words);
    RUN_TEST(wrap_hard_breaks);
    RUN_TEST(wrap_no_blank_line_after_trailing_space);
    RUN_TEST(wrap_breaks_long_words);
    RUN_TEST(wrap_ellipsizes_last_line);
    RUN_TEST(wrap_ellipsizes_before_hidden_paragraph);
    RUN_TEST(wrap_exact_fit_has_no_ellipsis);
    RUN_TEST(wrap_lines_fit_width);
    RUN_TEST(wrap_keeps_utf8_whole);
    RUN_TEST(wrap_caps_line_bytes);
}
