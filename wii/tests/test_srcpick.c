#include "greatest/greatest.h"
#include "srcpick.h"

TEST after_server_ok_uses_server(void)
{
    ASSERT_EQ(SP_SERVER, sp_after_server(SRV_OK, false));
    ASSERT_EQ(SP_SERVER, sp_after_server(SRV_OK, true));
    PASS();
}

TEST after_server_failure_falls_back_to_drive(void)
{
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_NO_ANSWER, true));
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_BAD_LIBRARY, true));
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_NOT_CABLE_TWII, true));
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_NO_NETWORK, true));
    PASS();
}

TEST after_server_failure_without_drive_fails(void)
{
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_NO_ANSWER, false));
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_BAD_LIBRARY, false));
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_NOT_CABLE_TWII, false));
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_NO_NETWORK, false));
    PASS();
}

TEST try_server_needs_server_source_and_valid_address(void)
{
    ctw_settings drive = { SRC_DRIVE, "192.168.1.20" };
    ctw_settings bad = { SRC_SERVER, "https://x" };
    ctw_settings good = { SRC_SERVER, "192.168.1.20:4747" };
    ASSERT_FALSE(sp_try_server(&drive));
    ASSERT_FALSE(sp_try_server(&bad));
    ASSERT(sp_try_server(&good));
    PASS();
}

TEST fallback_notice_names_the_drive(void)
{
    ASSERT_STR_EQ("Server not reachable, playing from USB", sp_fallback_notice(VOL_USB));
    ASSERT_STR_EQ("Server not reachable, playing from the SD card", sp_fallback_notice(VOL_SD));
    PASS();
}

TEST fail_body_names_the_address(void)
{
    char out[256];
    const char *expected = "cable-twii couldn't connect to 192.168.1.20:4747. Check that the server is running and the Wii is online, or change the address in Settings.";
    sp_fail_body(SRV_NO_ANSWER, "192.168.1.20:4747", out, sizeof out);
    ASSERT_STR_EQ(expected, out);
    sp_fail_body(SRV_NOT_CABLE_TWII, "192.168.1.20:4747", out, sizeof out);
    ASSERT_STR_EQ(expected, out);
    sp_fail_body(SRV_BAD_LIBRARY, "192.168.1.20:4747", out, sizeof out);
    ASSERT_STR_EQ(expected, out);
    PASS();
}

TEST fail_body_without_network(void)
{
    char out[256];
    sp_fail_body(SRV_NO_NETWORK, "192.168.1.20:4747", out, sizeof out);
    ASSERT_STR_EQ("Can't reach the network. Check the Wii's internet settings, or start cable-twii again.", out);
    PASS();
}

TEST connecting_names_the_address(void)
{
    char out[128];
    sp_connecting("192.168.1.20:4747", out, sizeof out);
    ASSERT_STR_EQ("Connecting to 192.168.1.20:4747\xE2\x80\xA6", out);
    PASS();
}

SUITE(srcpick_suite)
{
    RUN_TEST(after_server_ok_uses_server);
    RUN_TEST(after_server_failure_falls_back_to_drive);
    RUN_TEST(after_server_failure_without_drive_fails);
    RUN_TEST(try_server_needs_server_source_and_valid_address);
    RUN_TEST(fallback_notice_names_the_drive);
    RUN_TEST(fail_body_names_the_address);
    RUN_TEST(fail_body_without_network);
    RUN_TEST(connecting_names_the_address);
}
