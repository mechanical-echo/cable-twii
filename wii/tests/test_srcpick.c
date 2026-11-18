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

SUITE(srcpick_suite)
{
    RUN_TEST(after_server_ok_uses_server);
    RUN_TEST(after_server_failure_falls_back_to_drive);
    RUN_TEST(after_server_failure_without_drive_fails);
    RUN_TEST(try_server_needs_server_source_and_valid_address);
}
