#include <string.h>
#include "greatest/greatest.h"
#include "osd.h"

TEST chip_shows_two_seconds(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_channel_changed(&s, 1000);
    ASSERT(osd_chip_visible(&s, 2999));
    ASSERT(!osd_chip_visible(&s, 3000));
    PASS();
}

TEST osd_shows_on_motion(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    ASSERT(osd_visible(&s, 3999));
    ASSERT(!osd_visible(&s, 4000));
    PASS();
}

TEST osd_hidden_without_ir(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    osd_pointer(&s, false, false, 1100);
    ASSERT(!osd_visible(&s, 1100));
    PASS();
}

TEST osd_still_pointer_times_out(void)
{
    osd_state s;
    memset(&s, 0, sizeof s);
    osd_pointer(&s, true, true, 1000);
    osd_pointer(&s, true, false, 2000);
    ASSERT(!osd_visible(&s, 4000));
    PASS();
}

SUITE(osd_suite)
{
    RUN_TEST(chip_shows_two_seconds);
    RUN_TEST(osd_shows_on_motion);
    RUN_TEST(osd_hidden_without_ir);
    RUN_TEST(osd_still_pointer_times_out);
}
