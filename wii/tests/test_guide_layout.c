#include "greatest/greatest.h"
#include "guide_layout.h"

#define EPS 0.01f

TEST layout_4_3_preview_is_16_9(void)
{
    guide_layout l = guide_layout_for(1.0f);
    ASSERT_IN_RANGE(19.2f, l.list.x, EPS);
    ASSERT_IN_RANGE(44.0f, l.list.y, EPS);
    ASSERT_IN_RANGE(252.672f, l.list.w, EPS);
    ASSERT_IN_RANGE(416.8f, l.list.h, EPS);
    ASSERT_IN_RANGE(289.92f, l.preview.x, EPS);
    ASSERT_IN_RANGE(44.0f, l.preview.y, EPS);
    ASSERT_IN_RANGE(330.88f, l.preview.w, EPS);
    ASSERT_IN_RANGE(186.12f, l.preview.h, EPS);
    ASSERT_IN_RANGE(289.92f, l.info.x, EPS);
    ASSERT_IN_RANGE(242.624f, l.info.y, EPS);
    ASSERT_IN_RANGE(330.88f, l.info.w, EPS);
    ASSERT_IN_RANGE(218.176f, l.info.h, EPS);
    PASS();
}

TEST layout_16_9_keeps_info_room(void)
{
    guide_layout l = guide_layout_for(0.75f);
    ASSERT_IN_RANGE(228.296f, l.preview.h, EPS);
    ASSERT_IN_RANGE(304.394f, l.preview.w, EPS);
    ASSERT_IN_RANGE(303.163f, l.preview.x, EPS);
    ASSERT_IN_RANGE(284.8f, l.info.y, EPS);
    ASSERT_IN_RANGE(176.0f, l.info.h, EPS);
    ASSERT_IN_RANGE(289.92f, l.info.x, EPS);
    ASSERT_IN_RANGE(330.88f, l.info.w, EPS);
    PASS();
}

SUITE(guide_layout_suite)
{
    RUN_TEST(layout_4_3_preview_is_16_9);
    RUN_TEST(layout_16_9_keeps_info_room);
}
