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

SUITE(textfit_suite)
{
    RUN_TEST(text_fits_whole);
    RUN_TEST(text_fit_ellipsizes);
    RUN_TEST(text_fit_utf8_boundary);
}
