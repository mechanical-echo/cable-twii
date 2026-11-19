#include "greatest/greatest.h"
#include "settings_layout.h"

#define EPS 0.1f
#define WIDE 0.75f

static void centre(asp_rect r, float *x, float *y)
{
    *x = r.x + r.w / 2.0f;
    *y = r.y + r.h / 2.0f;
}

static bool rect_is(asp_rect r, float x, float y, float w, float h)
{
    return r.x > x - EPS && r.x < x + EPS && r.y > y - EPS && r.y < y + EPS &&
           r.w > w - EPS && r.w < w + EPS && r.h > h - EPS && r.h < h + EPS;
}

#define ASSERT_RECT(r, x, y, w, h) ASSERT(rect_is((r), (x), (y), (w), (h)))

TEST setl_match_the_mockup_at_4_3(void)
{
    settings_layout l = setl_make(1.0f);
    ASSERT_RECT(l.drive, 216, 115, 120, 40);
    ASSERT_RECT(l.server, 336, 115, 120, 40);
    ASSERT_RECT(l.field, 216, 169, 254, 40);
    ASSERT_RECT(l.edit, 480, 169, 76, 40);
    ASSERT_RECT(l.test, 84, 223, 170, 40);
    ASSERT_RECT(l.result, 100, 273, 456, 37.2f);
    ASSERT_RECT(l.cancel, 306, 369, 120, 40);
    ASSERT_RECT(l.save, 436, 369, 120, 40);
    ASSERT_RECT(l.text, 63, 110, 514, 44);
    PASS();
}

TEST setl_match_the_mockup_at_16_9(void)
{
    settings_layout l = setl_make(WIDE);
    ASSERT_RECT(l.drive, 216, 115, 120, 40);
    ASSERT_RECT(l.server, 336, 115, 120, 40);
    ASSERT_RECT(l.field, 216, 169, 254, 40);
    ASSERT_RECT(l.edit, 480, 169, 76, 40);
    ASSERT_RECT(l.test, 84, 223, 170, 40);
    ASSERT_RECT(l.result, 98, 273, 458, 37.2f);
    ASSERT_RECT(l.cancel, 306, 369, 120, 40);
    ASSERT_RECT(l.save, 436, 369, 120, 40);
    ASSERT_RECT(l.text, 63, 110, 514, 44);
    PASS();
}

TEST setl_keys_match_the_mockup(void)
{
    for (int i = 0; i < 2; i++) {
        settings_layout l = setl_make(i == 0 ? 1.0f : WIDE);
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 10; c++) {
                ASSERT_RECT(l.keys[r][c], 63.0f + 52.0f * (float)c, 168.0f + 46.0f * (float)r, 46, 40);
            }
        }
        ASSERT_RECT(l.keys[4][0], 63, 352, 150, 40);
        ASSERT_RECT(l.keys[4][1], 219, 352, 150, 40);
        ASSERT_RECT(l.keys[4][2], 375, 352, 202, 40);
    }
    PASS();
}

TEST setl_hit_finds_each_button_at_its_centre(void)
{
    for (int i = 0; i < 2; i++) {
        settings_layout l = setl_make(i == 0 ? 1.0f : WIDE);
        float x, y;
        centre(l.drive, &x, &y);
        ASSERT_EQ(SET_BTN_DRIVE, setl_hit(&l, x, y));
        centre(l.server, &x, &y);
        ASSERT_EQ(SET_BTN_SERVER, setl_hit(&l, x, y));
        centre(l.edit, &x, &y);
        ASSERT_EQ(SET_BTN_EDIT, setl_hit(&l, x, y));
        centre(l.field, &x, &y);
        ASSERT_EQ(SET_BTN_EDIT, setl_hit(&l, x, y));
        centre(l.test, &x, &y);
        ASSERT_EQ(SET_BTN_TEST, setl_hit(&l, x, y));
        centre(l.save, &x, &y);
        ASSERT_EQ(SET_BTN_SAVE, setl_hit(&l, x, y));
        centre(l.cancel, &x, &y);
        ASSERT_EQ(SET_BTN_CANCEL, setl_hit(&l, x, y));
    }
    PASS();
}

TEST setl_hit_misses_the_card_and_the_outside(void)
{
    settings_layout l = setl_make(1.0f);
    ASSERT_EQ(SET_BTN_NONE, setl_hit(&l, 2.0f, 2.0f));
    ASSERT_EQ(SET_BTN_NONE, setl_hit(&l, 320.0f, 60.0f));
    ASSERT_EQ(SET_BTN_NONE, setl_hit(&l, 320.0f, 300.0f));
    PASS();
}

TEST setl_key_hit_finds_every_key_at_its_centre(void)
{
    settings_layout l = setl_make(1.0f);
    for (int r = 0; r < KB_ROWS; r++) {
        for (int c = 0; c < kb_row_len(r); c++) {
            float x, y;
            int hr = -1, hc = -1;
            centre(l.keys[r][c], &x, &y);
            ASSERT(setl_key_hit(&l, x, y, &hr, &hc));
            ASSERT_EQ(r, hr);
            ASSERT_EQ(c, hc);
        }
    }
    PASS();
}

TEST setl_key_hit_has_no_dead_spots_in_the_gaps(void)
{
    settings_layout l = setl_make(1.0f);
    int r = -1, c = -1;
    ASSERT(setl_key_hit(&l, 63.0f + 46.0f + 3.0f, 188.0f, &r, &c));
    ASSERT_EQ(0, r);
    ASSERT(c == 0 || c == 1);
    ASSERT(setl_key_hit(&l, 83.0f, 168.0f + 40.0f + 3.0f, &r, &c));
    ASSERT(r == 0 || r == 1);
    PASS();
}

TEST setl_key_hit_misses_outside_the_grid(void)
{
    settings_layout l = setl_make(1.0f);
    int r = -1, c = -1;
    ASSERT_FALSE(setl_key_hit(&l, 20.0f, 188.0f, &r, &c));
    ASSERT_FALSE(setl_key_hit(&l, 320.0f, 440.0f, &r, &c));
    ASSERT_FALSE(setl_key_hit(&l, 320.0f, 140.0f, &r, &c));
    PASS();
}

SUITE(settings_layout_suite)
{
    RUN_TEST(setl_match_the_mockup_at_4_3);
    RUN_TEST(setl_match_the_mockup_at_16_9);
    RUN_TEST(setl_keys_match_the_mockup);
    RUN_TEST(setl_hit_finds_each_button_at_its_centre);
    RUN_TEST(setl_hit_misses_the_card_and_the_outside);
    RUN_TEST(setl_key_hit_finds_every_key_at_its_centre);
    RUN_TEST(setl_key_hit_has_no_dead_spots_in_the_gaps);
    RUN_TEST(setl_key_hit_misses_outside_the_grid);
}
