#include "greatest/greatest.h"
#include "preview.h"

TEST pv_tunes_after_highlight_settles(void)
{
    pv_state s;
    pv_reset(&s, 0);
    ASSERT_EQ(0, pv_update(&s, 2, 1000));
    ASSERT_EQ(0, pv_update(&s, 2, 1299));
    ASSERT_EQ(2, pv_update(&s, 2, 1300));
    ASSERT_EQ(0, pv_update(&s, 2, 1316));
    ASSERT_EQ(0, pv_update(&s, 2, 5000));
    PASS();
}

TEST pv_hover_sweep_does_not_thrash(void)
{
    pv_state s;
    pv_reset(&s, 1);
    uint32_t now = 1000;
    for (int ch = 2; ch <= 9; ch++) {
        for (int frame = 0; frame < 6; frame++) {
            ASSERT_EQ(0, pv_update(&s, ch, now));
            now += 16;
        }
    }
    ASSERT_EQ(0, pv_update(&s, 9, now + 200));
    ASSERT_EQ(9, pv_update(&s, 9, now - 96 + 300));
    PASS();
}

TEST pv_return_to_tuned_channel_keeps_stream(void)
{
    pv_state s;
    pv_reset(&s, 3);
    ASSERT_EQ(0, pv_update(&s, 4, 1000));
    ASSERT_EQ(0, pv_update(&s, 3, 1100));
    ASSERT_EQ(0, pv_update(&s, 3, 2000));
    PASS();
}

TEST pv_none_highlighted_never_tunes(void)
{
    pv_state s;
    pv_reset(&s, 0);
    ASSERT_EQ(0, pv_update(&s, 0, 1000));
    ASSERT_EQ(0, pv_update(&s, 0, 5000));
    PASS();
}

TEST pv_tuned_reports_and_forced(void)
{
    pv_state s;
    pv_reset(&s, 0);
    ASSERT_EQ(0, pv_update(&s, 5, 1000));
    pv_force(&s, 5);
    ASSERT_EQ(5, pv_tuned(&s));
    ASSERT_EQ(0, pv_update(&s, 5, 2000));
    PASS();
}

TEST pv_debounce_is_300ms(void)
{
    ASSERT_EQ(300u, PV_DEBOUNCE_MS);
    PASS();
}

SUITE(preview_suite)
{
    RUN_TEST(pv_tunes_after_highlight_settles);
    RUN_TEST(pv_hover_sweep_does_not_thrash);
    RUN_TEST(pv_return_to_tuned_channel_keeps_stream);
    RUN_TEST(pv_none_highlighted_never_tunes);
    RUN_TEST(pv_tuned_reports_and_forced);
    RUN_TEST(pv_debounce_is_300ms);
}
