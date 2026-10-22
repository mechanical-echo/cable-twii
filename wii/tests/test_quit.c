#include "greatest/greatest.h"
#include "quit.h"

static const unsigned char stub_mem[12] = { 0, 0, 0, 0, 'S', 'T', 'U', 'B', 'H', 'A', 'X', 'X' };
static const unsigned char empty_mem[12] = { 0 };

TEST loader_stub_returns_to_loader(void)
{
    ASSERT_EQ(QUIT_LOADER, quit_route_for(QUIT_DEST_HOMEBREW, true, false));
    PASS();
}

TEST emulator_powers_off_even_with_stub(void)
{
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_HOMEBREW, false, true));
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_HOMEBREW, true, true));
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_WII_MENU, false, true));
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_WII_MENU, true, true));
    PASS();
}

TEST homebrew_without_stub_falls_back_to_wii_menu(void)
{
    ASSERT_EQ(QUIT_WII_MENU, quit_route_for(QUIT_DEST_HOMEBREW, false, false));
    PASS();
}

TEST wii_menu_goes_to_wii_menu_even_with_stub(void)
{
    ASSERT_EQ(QUIT_WII_MENU, quit_route_for(QUIT_DEST_WII_MENU, true, false));
    ASSERT_EQ(QUIT_WII_MENU, quit_route_for(QUIT_DEST_WII_MENU, false, false));
    PASS();
}

TEST power_off_request_powers_off_everywhere(void)
{
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_POWER_OFF, true, false));
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_POWER_OFF, false, false));
    ASSERT_EQ(QUIT_POWER_OFF, quit_route_for(QUIT_DEST_POWER_OFF, true, true));
    PASS();
}

TEST stub_detected_from_signature(void)
{
    ASSERT(quit_stub_present(stub_mem));
    ASSERT_FALSE(quit_stub_present(empty_mem));
    PASS();
}

SUITE(quit_suite)
{
    RUN_TEST(loader_stub_returns_to_loader);
    RUN_TEST(emulator_powers_off_even_with_stub);
    RUN_TEST(homebrew_without_stub_falls_back_to_wii_menu);
    RUN_TEST(wii_menu_goes_to_wii_menu_even_with_stub);
    RUN_TEST(power_off_request_powers_off_everywhere);
    RUN_TEST(stub_detected_from_signature);
}
