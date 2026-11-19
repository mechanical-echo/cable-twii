#include "greatest/greatest.h"
#include "srcpick.h"

TEST after_server_ok_uses_server(void)
{
    ASSERT_EQ(SP_SERVER, sp_after_server(SRV_OK, false, false));
    ASSERT_EQ(SP_SERVER, sp_after_server(SRV_OK, false, true));
    PASS();
}

TEST after_server_failure_falls_back_to_drive(void)
{
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_NO_ANSWER, false, true));
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_BAD_LIBRARY, false, true));
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_NOT_CABLE_TWII, false, true));
    ASSERT_EQ(SP_DRIVE_WITH_NOTICE, sp_after_server(SRV_NO_NETWORK, false, true));
    PASS();
}

TEST after_server_failure_without_drive_fails(void)
{
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_NO_ANSWER, false, false));
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_BAD_LIBRARY, false, false));
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_NOT_CABLE_TWII, false, false));
    ASSERT_EQ(SP_FAIL, sp_after_server(SRV_NO_NETWORK, false, false));
    PASS();
}

TEST after_reload_ok_swaps_to_the_new_library(void)
{
    ASSERT_EQ(SP_SERVER, sp_after_server(SRV_OK, true, false));
    ASSERT_EQ(SP_SERVER, sp_after_server(SRV_OK, true, true));
    PASS();
}

TEST after_reload_failure_keeps_the_library(void)
{
    srv_result failures[] = { SRV_NO_ANSWER, SRV_BAD_LIBRARY, SRV_NOT_CABLE_TWII, SRV_NO_NETWORK };
    for (size_t i = 0; i < sizeof failures / sizeof failures[0]; i++) {
        ASSERT_EQ(SP_KEEP, sp_after_server(failures[i], true, false));
        ASSERT_EQ(SP_KEEP, sp_after_server(failures[i], true, true));
    }
    PASS();
}

TEST reload_notice_on_success(void)
{
    ASSERT_STR_EQ("Channels updated", sp_reload_notice(SRV_OK, true));
    ASSERT_STR_EQ("Channels updated", sp_reload_notice(SRV_OK, false));
    PASS();
}

TEST manual_reload_failure_cant_reach_the_server(void)
{
    srv_result failures[] = { SRV_NO_ANSWER, SRV_NOT_CABLE_TWII, SRV_NO_NETWORK };
    for (size_t i = 0; i < sizeof failures / sizeof failures[0]; i++) {
        ASSERT_STR_EQ("Can't reach the server", sp_reload_notice(failures[i], true));
    }
    PASS();
}

TEST manual_reload_with_bad_channels_names_the_problem(void)
{
    ASSERT_STR_EQ("The server's channels have a problem. Open cable-twii Studio on the server to fix it.", sp_reload_notice(SRV_BAD_LIBRARY, true));
    PASS();
}

TEST automatic_reload_failure_is_silent(void)
{
    srv_result failures[] = { SRV_NO_ANSWER, SRV_BAD_LIBRARY, SRV_NOT_CABLE_TWII, SRV_NO_NETWORK };
    for (size_t i = 0; i < sizeof failures / sizeof failures[0]; i++) {
        ASSERT_EQ(NULL, sp_reload_notice(failures[i], false));
    }
    PASS();
}

TEST try_server_needs_server_source_and_valid_address(void)
{
    ctw_settings drive = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ctw_settings bad = { SRC_SERVER, "https://x", ASP_FIT };
    ctw_settings good = { SRC_SERVER, "192.168.1.20:4747", ASP_FIT };
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

TEST test_start_connects_to_the_parsed_address(void)
{
    srv_addr addr = { "", 0 };
    char name[128], line[320];
    ASSERT_EQ(SP_LINE_BUSY, sp_test_start("http://192.168.1.20/", &addr, name, sizeof name, line, sizeof line));
    ASSERT_STR_EQ("192.168.1.20", addr.host);
    ASSERT_EQ(4747, addr.port);
    ASSERT_STR_EQ("192.168.1.20:4747", name);
    ASSERT_STR_EQ("Connecting to 192.168.1.20:4747\xE2\x80\xA6", line);
    PASS();
}

TEST test_start_without_a_valid_address_has_no_answer(void)
{
    srv_addr addr = { "", 0 };
    char name[128], line[320];
    ASSERT_EQ(SP_LINE_FAIL, sp_test_start("  https://pc  ", &addr, name, sizeof name, line, sizeof line));
    ASSERT_STR_EQ("https://pc", name);
    ASSERT_STR_EQ("No answer from https://pc. Check that the server is running, the address is right and the PC's firewall allows it.", line);
    ASSERT_STR_EQ("", addr.host);
    PASS();
}

TEST test_start_with_an_empty_address_shows_nothing(void)
{
    srv_addr addr = { "", 0 };
    char name[128], line[320];
    ASSERT_EQ(SP_LINE_NONE, sp_test_start("", &addr, name, sizeof name, line, sizeof line));
    ASSERT_STR_EQ("", line);
    ASSERT_EQ(SP_LINE_NONE, sp_test_start(" \t ", &addr, name, sizeof name, line, sizeof line));
    ASSERT_STR_EQ("", name);
    ASSERT_STR_EQ("", line);
    PASS();
}

TEST test_result_connected_names_the_server(void)
{
    char line[320];
    ASSERT_EQ(SP_LINE_OK, sp_test_result(SRV_OK, "192.168.1.20:4747", "Home PC", line, sizeof line));
    ASSERT_STR_EQ("Connected to \"Home PC\".", line);
    PASS();
}

TEST test_result_failures_use_the_test_copy(void)
{
    char line[320];
    ASSERT_EQ(SP_LINE_FAIL, sp_test_result(SRV_NO_ANSWER, "192.168.1.20:4747", "", line, sizeof line));
    ASSERT_STR_EQ("No answer from 192.168.1.20:4747. Check that the server is running, the address is right and the PC's firewall allows it.", line);
    ASSERT_EQ(SP_LINE_FAIL, sp_test_result(SRV_NOT_CABLE_TWII, "192.168.1.20:4747", "", line, sizeof line));
    ASSERT_STR_EQ("192.168.1.20:4747 answered, but it isn't a cable-twii server.", line);
    ASSERT_EQ(SP_LINE_FAIL, sp_test_result(SRV_BAD_LIBRARY, "192.168.1.20:4747", "", line, sizeof line));
    ASSERT_STR_EQ("The server's channels have a problem. Open cable-twii Studio on the server to fix it.", line);
    ASSERT_EQ(SP_LINE_FAIL, sp_test_result(SRV_NO_NETWORK, "192.168.1.20:4747", "", line, sizeof line));
    ASSERT_STR_EQ("Can't reach the network. Check the Wii's internet settings, or start cable-twii again.", line);
    PASS();
}

TEST save_with_server_and_no_address_asks_for_one(void)
{
    ctw_settings was = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings empty = { SRC_SERVER, "", ASP_FIT };
    ctw_settings spaces = { SRC_SERVER, " \t ", ASP_FIT };
    ASSERT_EQ(SP_SAVE_TYPE, sp_save_plan(&was, &empty, false, false));
    ASSERT_EQ(SP_SAVE_TYPE, sp_save_plan(&was, &spaces, false, false));
    ASSERT_EQ(SP_SAVE_TYPE, sp_save_plan(&empty, &empty, true, false));
    PASS();
}

TEST save_with_server_and_an_unusable_address_refuses(void)
{
    ctw_settings was = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings bad = { SRC_SERVER, "https://pc", ASP_FIT };
    ASSERT_EQ(SP_SAVE_BAD, sp_save_plan(&was, &bad, false, false));
    ASSERT_EQ(SP_SAVE_BAD, sp_save_plan(&bad, &bad, false, false));
    ASSERT_EQ(SP_SAVE_BAD, sp_save_plan(&bad, &bad, true, false));
    PASS();
}

TEST save_with_nothing_changed_returns(void)
{
    ctw_settings server = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings drive = { SRC_DRIVE, "", ASP_FIT };
    ASSERT_EQ(SP_SAVE_RETURN, sp_save_plan(&server, &server, false, true));
    ASSERT_EQ(SP_SAVE_RETURN, sp_save_plan(&drive, &drive, false, false));
    PASS();
}

TEST save_with_nothing_changed_on_the_error_screen_retries(void)
{
    ctw_settings server = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings drive = { SRC_DRIVE, "", ASP_FIT };
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&server, &server, true, false));
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&drive, &drive, true, false));
    PASS();
}

TEST save_with_nothing_changed_on_the_drive_fallback_retries_the_server(void)
{
    ctw_settings server = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&server, &server, false, false));
    PASS();
}

TEST save_switches_when_the_source_restarts(void)
{
    ctw_settings drive = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ctw_settings server = { SRC_SERVER, "192.168.1.20", ASP_FIT };
    ctw_settings moved = { SRC_SERVER, "192.168.1.21:4748", ASP_FIT };
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&drive, &server, false, false));
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&server, &drive, false, true));
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&server, &moved, false, true));
    PASS();
}

TEST save_in_drive_mode_stores_any_address(void)
{
    ctw_settings was = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings typed = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ctw_settings bad = { SRC_DRIVE, "https://pc", ASP_FIT };
    ASSERT_EQ(SP_SAVE_STORE, sp_save_plan(&was, &typed, false, false));
    ASSERT_EQ(SP_SAVE_STORE, sp_save_plan(&was, &bad, false, false));
    PASS();
}

TEST save_in_drive_mode_on_the_error_screen_retries(void)
{
    ctw_settings was = { SRC_DRIVE, "", ASP_FIT };
    ctw_settings typed = { SRC_DRIVE, "192.168.1.20", ASP_FIT };
    ASSERT_EQ(SP_SAVE_SWITCH, sp_save_plan(&was, &typed, true, false));
    PASS();
}

SUITE(srcpick_suite)
{
    RUN_TEST(after_server_ok_uses_server);
    RUN_TEST(after_server_failure_falls_back_to_drive);
    RUN_TEST(after_server_failure_without_drive_fails);
    RUN_TEST(after_reload_ok_swaps_to_the_new_library);
    RUN_TEST(after_reload_failure_keeps_the_library);
    RUN_TEST(reload_notice_on_success);
    RUN_TEST(manual_reload_failure_cant_reach_the_server);
    RUN_TEST(manual_reload_with_bad_channels_names_the_problem);
    RUN_TEST(automatic_reload_failure_is_silent);
    RUN_TEST(try_server_needs_server_source_and_valid_address);
    RUN_TEST(fallback_notice_names_the_drive);
    RUN_TEST(fail_body_names_the_address);
    RUN_TEST(fail_body_without_network);
    RUN_TEST(connecting_names_the_address);
    RUN_TEST(test_start_connects_to_the_parsed_address);
    RUN_TEST(test_start_without_a_valid_address_has_no_answer);
    RUN_TEST(test_start_with_an_empty_address_shows_nothing);
    RUN_TEST(test_result_connected_names_the_server);
    RUN_TEST(test_result_failures_use_the_test_copy);
    RUN_TEST(save_with_server_and_no_address_asks_for_one);
    RUN_TEST(save_with_server_and_an_unusable_address_refuses);
    RUN_TEST(save_with_nothing_changed_returns);
    RUN_TEST(save_with_nothing_changed_on_the_error_screen_retries);
    RUN_TEST(save_with_nothing_changed_on_the_drive_fallback_retries_the_server);
    RUN_TEST(save_switches_when_the_source_restarts);
    RUN_TEST(save_in_drive_mode_stores_any_address);
    RUN_TEST(save_in_drive_mode_on_the_error_screen_retries);
}
