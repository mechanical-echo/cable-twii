#include "greatest/greatest.h"
#include <string.h>
#include "keyboard.h"

static void type(kb_state *k, char c)
{
    kb_key key = { KB_KEY_CHAR, c };
    kb_press(k, key);
}

TEST kb_types_and_backs(void)
{
    kb_state k;
    kb_init(&k, "");
    type(&k, '1');
    type(&k, '.');
    kb_key back = { KB_KEY_BACK, 0 };
    ASSERT_FALSE(kb_press(&k, back));
    ASSERT_STR_EQ("1", k.text);
    ASSERT_EQ(1, (int)k.len);
    PASS();
}

TEST kb_back_on_empty_stays_empty(void)
{
    kb_state k;
    kb_init(&k, "");
    kb_key back = { KB_KEY_BACK, 0 };
    kb_press(&k, back);
    ASSERT_STR_EQ("", k.text);
    PASS();
}

TEST kb_http_only_when_empty(void)
{
    kb_state k;
    kb_key http = { KB_KEY_HTTP, 0 };
    kb_init(&k, "");
    kb_press(&k, http);
    ASSERT_STR_EQ("http://", k.text);
    kb_init(&k, "1");
    kb_press(&k, http);
    ASSERT_STR_EQ("1", k.text);
    PASS();
}

TEST kb_stops_at_max(void)
{
    kb_state k;
    kb_init(&k, "");
    for (int i = 0; i < KB_MAX_TEXT + 10; i++) type(&k, 'a');
    ASSERT_EQ(KB_MAX_TEXT, (int)k.len);
    ASSERT_EQ(KB_MAX_TEXT, (int)strlen(k.text));
    PASS();
}

TEST kb_init_keeps_text_and_cuts_at_max(void)
{
    kb_state k;
    kb_init(&k, "10.0.0.5:4747");
    ASSERT_STR_EQ("10.0.0.5:4747", k.text);
    ASSERT_EQ(13, (int)k.len);
    ASSERT_EQ(0, k.row);
    ASSERT_EQ(0, k.col);
    PASS();
}

TEST kb_move_clamps_to_short_row(void)
{
    kb_state k;
    kb_init(&k, "");
    k.row = 1;
    k.col = kb_row_len(1) - 1;
    kb_move(&k, 0, 1);
    kb_move(&k, 0, 1);
    kb_move(&k, 0, 1);
    ASSERT_EQ(KB_ROWS - 1, k.row);
    ASSERT_EQ(kb_row_len(KB_ROWS - 1) - 1, k.col);
    kb_move(&k, 0, 1);
    ASSERT_EQ(KB_ROWS - 1, k.row);
    PASS();
}

TEST kb_move_clamps_at_edges(void)
{
    kb_state k;
    kb_init(&k, "");
    kb_move(&k, -1, -1);
    ASSERT_EQ(0, k.row);
    ASSERT_EQ(0, k.col);
    kb_move(&k, 20, 0);
    ASSERT_EQ(9, k.col);
    PASS();
}

TEST kb_keys_follow_the_mockup(void)
{
    ASSERT_EQ(10, kb_row_len(0));
    ASSERT_EQ(10, kb_row_len(3));
    ASSERT_EQ(3, kb_row_len(4));
    ASSERT_EQ('1', kb_key_at(0, 0).ch);
    ASSERT_EQ('0', kb_key_at(0, 9).ch);
    ASSERT_EQ('q', kb_key_at(1, 0).ch);
    ASSERT_EQ('.', kb_key_at(2, 9).ch);
    ASSERT_EQ('z', kb_key_at(3, 0).ch);
    ASSERT_EQ('-', kb_key_at(3, 9).ch);
    ASSERT_EQ(KB_KEY_HTTP, kb_key_at(4, 0).kind);
    ASSERT_EQ(KB_KEY_BACK, kb_key_at(4, 1).kind);
    ASSERT_EQ(KB_KEY_DONE, kb_key_at(4, 2).kind);
    PASS();
}

TEST kb_done_returns_true(void)
{
    kb_state k;
    kb_init(&k, "a");
    kb_key done = { KB_KEY_DONE, 0 };
    ASSERT(kb_press(&k, done));
    ASSERT_STR_EQ("a", k.text);
    PASS();
}

SUITE(keyboard_suite)
{
    RUN_TEST(kb_types_and_backs);
    RUN_TEST(kb_back_on_empty_stays_empty);
    RUN_TEST(kb_http_only_when_empty);
    RUN_TEST(kb_stops_at_max);
    RUN_TEST(kb_init_keeps_text_and_cuts_at_max);
    RUN_TEST(kb_move_clamps_to_short_row);
    RUN_TEST(kb_move_clamps_at_edges);
    RUN_TEST(kb_keys_follow_the_mockup);
    RUN_TEST(kb_done_returns_true);
}
