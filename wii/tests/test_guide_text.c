#include <string.h>
#include "greatest/greatest.h"
#include "guide_text.h"

#define EPS 0.001f

static ctw_episode ep = { "Kids/Gumball/s01e04.mpg", "The Quest", "Desc", 1, 4, 660000 };
static ctw_show show = { "Kids", "Gumball", "Gumball", "Kids/Gumball/show.png", &ep, 1 };
static ctw_show bare = { "Kids", "Bare", "Bare Show", "", &ep, 1 };
static ctw_ad ad = { "Kids/ads/a.mpg", "Kids", 30000 };
static ctw_channel ch = { 1, "Retro", "Kids", "Kids", "Kids/channel.png", true, CTW_MODE_LOOPING, NULL, 0, NULL, 0 };

static ctw_airing episode_airing(int64_t start, uint32_t dur)
{
    ctw_airing a;
    memset(&a, 0, sizeof a);
    a.start_ms = start;
    a.duration_ms = dur;
    a.kind = CTW_AIR_EPISODE;
    a.show = &show;
    a.episode = &ep;
    return a;
}

TEST gt_hhmm_formats_and_wraps(void)
{
    char out[16];
    gt_hhmm(14 * 3600000LL + 30 * 60000LL, out, sizeof out);
    ASSERT_STR_EQ("14:30", out);
    gt_hhmm(24 * 3600000LL + 5 * 60000LL, out, sizeof out);
    ASSERT_STR_EQ("00:05", out);
    gt_hhmm(-60000LL, out, sizeof out);
    ASSERT_STR_EQ("23:59", out);
    PASS();
}

TEST gt_times_range(void)
{
    char out[32];
    gt_times(14 * 3600000LL + 30 * 60000LL, 11 * 60000u, out, sizeof out);
    ASSERT_STR_EQ("14:30 \xE2\x80\x93 14:41", out);
    gt_times(23 * 3600000LL + 50 * 60000LL, 20 * 60000u, out, sizeof out);
    ASSERT_STR_EQ("23:50 \xE2\x80\x93 00:10", out);
    PASS();
}

TEST gt_episode_code_and_title(void)
{
    char out[64];
    gt_episode(&ep, out, sizeof out);
    ASSERT_STR_EQ("S01E04 \xC2\xB7 The Quest", out);
    ctw_episode untitled = ep;
    untitled.title = "";
    gt_episode(&untitled, out, sizeof out);
    ASSERT_STR_EQ("S01E04", out);
    PASS();
}

TEST gt_line_per_kind(void)
{
    char out[128];
    ctw_airing a = episode_airing(0, 660000);
    gt_line(&a, NULL, out, sizeof out);
    ASSERT_STR_EQ("Gumball \xC2\xB7 S01E04 The Quest", out);

    ctw_airing c;
    memset(&c, 0, sizeof c);
    c.kind = CTW_AIR_AD;
    c.ad = &ad;
    gt_line(&c, NULL, out, sizeof out);
    ASSERT_STR_EQ("Commercial break", out);

    ctw_airing gap;
    memset(&gap, 0, sizeof gap);
    gap.kind = CTW_AIR_UPNEXT;
    gt_line(&gap, &a, out, sizeof out);
    ASSERT_STR_EQ("Up next: Gumball", out);
    gt_line(&gap, &c, out, sizeof out);
    ASSERT_STR_EQ("Up next: Commercial break", out);
    gt_line(&gap, NULL, out, sizeof out);
    ASSERT_STR_EQ("Up next", out);

    gt_line(NULL, NULL, out, sizeof out);
    ASSERT_STR_EQ("", out);
    PASS();
}

TEST gt_progress_fraction(void)
{
    ctw_airing a = episode_airing(0, 1000);
    ASSERT_IN_RANGE(0.0f, gt_progress(&a, 0), EPS);
    ASSERT_IN_RANGE(0.25f, gt_progress(&a, 250), EPS);
    ASSERT_IN_RANGE(1.0f, gt_progress(&a, 2000), EPS);
    ctw_airing z = episode_airing(0, 0);
    ASSERT_IN_RANGE(0.0f, gt_progress(&z, 10), EPS);
    ASSERT_IN_RANGE(0.0f, gt_progress(NULL, 10), EPS);
    PASS();
}

TEST gt_line_show_picks_icon_show(void)
{
    ctw_airing a = episode_airing(0, 660000);
    ctw_airing gap;
    memset(&gap, 0, sizeof gap);
    gap.kind = CTW_AIR_UPNEXT;
    ctw_airing c;
    memset(&c, 0, sizeof c);
    c.kind = CTW_AIR_AD;
    c.ad = &ad;
    ASSERT_EQ(&show, gt_line_show(&a, NULL));
    ASSERT_EQ(&show, gt_line_show(&gap, &a));
    ASSERT_EQ(NULL, gt_line_show(&gap, &c));
    ASSERT_EQ(NULL, gt_line_show(&c, &a));
    ASSERT_EQ(NULL, gt_line_show(NULL, NULL));
    PASS();
}

TEST gt_icon_path_falls_back_to_channel(void)
{
    char out[128];
    gt_icon_path("sd:/cable-twii", &ch, &show, out, sizeof out);
    ASSERT_STR_EQ("sd:/cable-twii/Kids/Gumball/show.png", out);
    gt_icon_path("sd:/cable-twii", &ch, &bare, out, sizeof out);
    ASSERT_STR_EQ("sd:/cable-twii/Kids/channel.png", out);
    gt_icon_path("sd:/cable-twii", &ch, NULL, out, sizeof out);
    ASSERT_STR_EQ("sd:/cable-twii/Kids/channel.png", out);
    PASS();
}

SUITE(guide_text_suite)
{
    RUN_TEST(gt_hhmm_formats_and_wraps);
    RUN_TEST(gt_times_range);
    RUN_TEST(gt_episode_code_and_title);
    RUN_TEST(gt_line_per_kind);
    RUN_TEST(gt_progress_fraction);
    RUN_TEST(gt_line_show_picks_icon_show);
    RUN_TEST(gt_icon_path_falls_back_to_channel);
}
