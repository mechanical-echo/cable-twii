#include "greatest/greatest.h"
#include "guide_layout.h"

#define EPS 0.2f

TEST layout_16_9_matches_the_deck(void)
{
    guide_layout l = guide_layout_for(0.75f);
    ASSERT_IN_RANGE(19.2f, l.list.x, EPS);
    ASSERT_IN_RANGE(58.7f, l.list.y, EPS);
    ASSERT_IN_RANGE(268.9f, l.list.w, EPS);
    ASSERT_IN_RANGE(351.1f, l.list.h, EPS);
    ASSERT_IN_RANGE(307.35f, l.preview.x, EPS);
    ASSERT_IN_RANGE(58.7f, l.preview.y, EPS);
    ASSERT_IN_RANGE(313.45f, l.preview.w, EPS);
    ASSERT_IN_RANGE(235.1f, l.preview.h, EPS);
    ASSERT_IN_RANGE(307.35f, l.info.x, EPS);
    ASSERT_IN_RANGE(307.8f, l.info.y, EPS);
    ASSERT_IN_RANGE(313.45f, l.info.w, EPS);
    ASSERT_IN_RANGE(102.0f, l.info.h, EPS);
    PASS();
}

TEST layout_4_3_preview_is_16_9(void)
{
    guide_layout l = guide_layout_for(1.0f);
    ASSERT_IN_RANGE(19.2f, l.list.x, EPS);
    ASSERT_IN_RANGE(268.9f, l.list.w, EPS);
    ASSERT_IN_RANGE(313.45f, l.preview.w, EPS);
    ASSERT_IN_RANGE(176.3f, l.preview.h, EPS);
    ASSERT_IN_RANGE(313.45f, l.info.w, EPS);
    ASSERT_IN_RANGE(160.8f, l.info.h, EPS);
    ASSERT_IN_RANGE(409.8f, l.info.y + l.info.h, EPS);
    PASS();
}

TEST layout_header_is_48_tall(void)
{
    ASSERT_IN_RANGE(48.0f, GL_HEADER_H, EPS);
    ASSERT_IN_RANGE(58.7f, GL_TOP, EPS);
    ASSERT_IN_RANGE(409.8f, GL_BOTTOM, EPS);
    PASS();
}

SUITE(guide_layout_suite)
{
    RUN_TEST(layout_16_9_matches_the_deck);
    RUN_TEST(layout_4_3_preview_is_16_9);
    RUN_TEST(layout_header_is_48_tall);
}
