#include <string.h>
#include "greatest/greatest.h"
#include "media/audiobuf.h"

#define RING 16
#define CHUNK 4

static uint32_t ring[RING];
static uint32_t out[CHUNK];

TEST start_plays_first_and_fills_second(void)
{
    ab_state s;
    memset(&s, 0, sizeof s);
    ASSERT_EQ(0, ab_start(&s));
    ASSERT_EQ(1, ab_take(&s));
    PASS();
}

TEST takes_alternate(void)
{
    ab_state s;
    memset(&s, 0, sizeof s);
    ab_start(&s);
    ASSERT_EQ(1, ab_take(&s));
    ASSERT_EQ(0, ab_take(&s));
    ASSERT_EQ(1, ab_take(&s));
    ASSERT_EQ(0, ab_take(&s));
    PASS();
}

TEST restart_resets_order(void)
{
    ab_state s;
    memset(&s, 0, sizeof s);
    ab_start(&s);
    ab_take(&s);
    ab_take(&s);
    ab_take(&s);
    ASSERT_EQ(0, ab_start(&s));
    ASSERT_EQ(1, ab_take(&s));
    PASS();
}

TEST drain_full_chunk(void)
{
    for (uint32_t i = 0; i < RING; i++) ring[i] = i + 100;
    uint32_t r = 0;
    ASSERT_EQ(4u, ab_drain(out, CHUNK, ring, RING, &r, 10));
    ASSERT_EQ(4u, r);
    ASSERT_EQ(100u, out[0]);
    ASSERT_EQ(103u, out[3]);
    PASS();
}

TEST drain_short_pads_silence(void)
{
    for (uint32_t i = 0; i < RING; i++) ring[i] = i + 100;
    memset(out, 0xFF, sizeof out);
    uint32_t r = 5;
    ASSERT_EQ(2u, ab_drain(out, CHUNK, ring, RING, &r, 7));
    ASSERT_EQ(7u, r);
    ASSERT_EQ(105u, out[0]);
    ASSERT_EQ(106u, out[1]);
    ASSERT_EQ(0u, out[2]);
    ASSERT_EQ(0u, out[3]);
    PASS();
}

TEST drain_wraps_ring(void)
{
    for (uint32_t i = 0; i < RING; i++) ring[i] = i + 100;
    uint32_t r = 30;
    ASSERT_EQ(4u, ab_drain(out, CHUNK, ring, RING, &r, 40));
    ASSERT_EQ(34u, r);
    ASSERT_EQ(114u, out[0]);
    ASSERT_EQ(115u, out[1]);
    ASSERT_EQ(100u, out[2]);
    ASSERT_EQ(101u, out[3]);
    PASS();
}

TEST drain_empty_is_silence(void)
{
    memset(out, 0xFF, sizeof out);
    uint32_t r = 3;
    ASSERT_EQ(0u, ab_drain(out, CHUNK, ring, RING, &r, 3));
    ASSERT_EQ(3u, r);
    for (int i = 0; i < CHUNK; i++) ASSERT_EQ(0u, out[i]);
    PASS();
}

SUITE(audiobuf_suite)
{
    RUN_TEST(start_plays_first_and_fills_second);
    RUN_TEST(takes_alternate);
    RUN_TEST(restart_resets_order);
    RUN_TEST(drain_full_chunk);
    RUN_TEST(drain_short_pads_silence);
    RUN_TEST(drain_wraps_ring);
    RUN_TEST(drain_empty_is_silence);
}
