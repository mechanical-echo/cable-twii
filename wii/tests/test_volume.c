#include <string.h>
#include "greatest/greatest.h"
#include "volume.h"

static uint8_t sector[512];

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void make_fat32_boot(uint8_t *s)
{
    memset(s, 0, 512);
    s[0] = 0xEB;
    s[1] = 0x58;
    s[2] = 0x90;
    memcpy(s + 3, "MSDOS5.0", 8);
    s[11] = 0x00;
    s[12] = 0x02;
    s[13] = 8;
    s[14] = 32;
    s[16] = 2;
    memcpy(s + 0x52, "FAT32   ", 8);
    s[510] = 0x55;
    s[511] = 0xAA;
}

static void make_fat16_boot(uint8_t *s)
{
    make_fat32_boot(s);
    memset(s + 0x52, 0, 8);
    s[14] = 1;
    memcpy(s + 0x36, "FAT16   ", 8);
}

static void set_entry(uint8_t *s, int i, uint8_t type, uint32_t start, uint32_t count)
{
    uint8_t *e = s + 446 + i * 16;
    e[4] = type;
    put_le32(e + 8, start);
    put_le32(e + 12, count);
}

static void make_mbr(uint8_t *s)
{
    memset(s, 0, 512);
    s[510] = 0x55;
    s[511] = 0xAA;
}

TEST pick_flash_drive_after_games_drive(void)
{
    vol_candidate c[] = {
        { VOL_USB, false, false },
        { VOL_USB, true, true },
        { VOL_SD, true, true },
    };
    ASSERT_EQ(1, vol_pick_root(c, 3));
    PASS();
}

TEST pick_skips_fat_partition_without_root(void)
{
    vol_candidate c[] = {
        { VOL_USB, true, false },
        { VOL_USB, true, true },
    };
    ASSERT_EQ(1, vol_pick_root(c, 2));
    PASS();
}

TEST pick_sd_without_usb_root(void)
{
    vol_candidate c[] = {
        { VOL_USB, false, false },
        { VOL_USB, true, false },
        { VOL_SD, true, true },
    };
    ASSERT_EQ(2, vol_pick_root(c, 3));
    PASS();
}

TEST pick_prefers_usb_over_earlier_sd(void)
{
    vol_candidate c[] = {
        { VOL_SD, true, true },
        { VOL_USB, true, true },
    };
    ASSERT_EQ(1, vol_pick_root(c, 2));
    PASS();
}

TEST pick_nothing(void)
{
    vol_candidate c[] = {
        { VOL_USB, false, false },
        { VOL_SD, true, false },
    };
    ASSERT_EQ(-1, vol_pick_root(c, 2));
    ASSERT_EQ(-1, vol_pick_root(NULL, 0));
    PASS();
}

TEST pick_ignores_root_flag_on_non_fat(void)
{
    vol_candidate c[] = { { VOL_USB, false, true } };
    ASSERT_EQ(-1, vol_pick_root(c, 1));
    PASS();
}

TEST boot_sector_fat32_and_fat16(void)
{
    make_fat32_boot(sector);
    ASSERT(vol_boot_is_fat(sector));
    make_fat16_boot(sector);
    ASSERT(vol_boot_is_fat(sector));
    PASS();
}

TEST boot_sector_rejects_wbfs_ntfs_exfat(void)
{
    memset(sector, 0, sizeof sector);
    memcpy(sector, "WBFS", 4);
    ASSERT(!vol_boot_is_fat(sector));
    make_fat32_boot(sector);
    memcpy(sector + 3, "NTFS    ", 8);
    ASSERT(!vol_boot_is_fat(sector));
    make_fat32_boot(sector);
    memcpy(sector + 3, "EXFAT   ", 8);
    ASSERT(!vol_boot_is_fat(sector));
    make_fat32_boot(sector);
    sector[511] = 0;
    ASSERT(!vol_boot_is_fat(sector));
    make_fat32_boot(sector);
    sector[12] = 0;
    ASSERT(!vol_boot_is_fat(sector));
    make_mbr(sector);
    set_entry(sector, 0, 0x0C, 2048, 1000000);
    ASSERT(!vol_boot_is_fat(sector));
    PASS();
}

TEST mbr_lists_primary_entries(void)
{
    make_mbr(sector);
    set_entry(sector, 0, 0x0C, 2048, 1000);
    set_entry(sector, 2, 0x0F, 5000, 9000);
    vol_part_entry e[4];
    ASSERT(vol_mbr_valid(sector));
    ASSERT_EQ(2u, vol_mbr_entries(sector, e));
    ASSERT_EQ(0x0C, e[0].type);
    ASSERT_EQ(2048u, e[0].start);
    ASSERT_EQ(1000u, e[0].sectors);
    ASSERT_EQ(0x0F, e[1].type);
    ASSERT(vol_is_extended(e[1].type));
    ASSERT(!vol_is_extended(e[0].type));
    sector[510] = 0;
    ASSERT(!vol_mbr_valid(sector));
    PASS();
}

TEST ebr_gives_logical_and_next(void)
{
    make_mbr(sector);
    set_entry(sector, 0, 0x0B, 63, 1000);
    set_entry(sector, 1, 0x05, 3000, 2000);
    vol_part_entry logical;
    uint32_t next = 0;
    ASSERT(vol_ebr_parse(sector, 10000, 10000, &logical, &next));
    ASSERT_EQ(0x0B, logical.type);
    ASSERT_EQ(10063u, logical.start);
    ASSERT_EQ(13000u, next);

    set_entry(sector, 1, 0x00, 0, 0);
    ASSERT(vol_ebr_parse(sector, 10000, 13000, &logical, &next));
    ASSERT_EQ(13063u, logical.start);
    ASSERT_EQ(0u, next);
    PASS();
}

TEST boot_sector_counts_total_sectors(void)
{
    make_fat32_boot(sector);
    put_le32(sector + 32, 3000000);
    ASSERT_EQ(3000000u, vol_boot_sectors(sector));
    make_fat16_boot(sector);
    sector[19] = 0x10;
    sector[20] = 0x27;
    put_le32(sector + 32, 5000000);
    ASSERT_EQ(10000u, vol_boot_sectors(sector));
    make_fat32_boot(sector);
    ASSERT_EQ(0u, vol_boot_sectors(sector));
    PASS();
}

TEST write_span_is_smaller_of_entry_and_boot(void)
{
    ASSERT_EQ(1000u, vol_write_span(1000, 2000));
    ASSERT_EQ(1000u, vol_write_span(2000, 1000));
    ASSERT_EQ(1500u, vol_write_span(0, 1500));
    ASSERT_EQ(0u, vol_write_span(1000, 0));
    ASSERT_EQ(0u, vol_write_span(0, 0));
    PASS();
}

TEST write_inside_partition_allowed(void)
{
    ASSERT(vol_write_in_range(2048, 1000, 2048, 1));
    ASSERT(vol_write_in_range(2048, 1000, 2500, 64));
    ASSERT(vol_write_in_range(0, 1000, 0, 1000));
    PASS();
}

TEST write_at_partition_boundary(void)
{
    ASSERT(vol_write_in_range(2048, 1000, 3047, 1));
    ASSERT(vol_write_in_range(2048, 1000, 2048, 1000));
    ASSERT(!vol_write_in_range(2048, 1000, 3048, 1));
    PASS();
}

TEST write_before_partition_refused(void)
{
    ASSERT(!vol_write_in_range(2048, 1000, 2047, 1));
    ASSERT(!vol_write_in_range(2048, 1000, 0, 1));
    ASSERT(!vol_write_in_range(2048, 1000, 2000, 100));
    PASS();
}

TEST write_past_partition_end_refused(void)
{
    ASSERT(!vol_write_in_range(2048, 1000, 5000, 1));
    ASSERT(!vol_write_in_range(2048, 1000, 0xFFFFFFFFu, 1));
    ASSERT(!vol_write_in_range(0xFFFFFF00u, 0x100u, 0xFFFFFFFFu, 2));
    PASS();
}

TEST write_straddling_end_refused(void)
{
    ASSERT(!vol_write_in_range(2048, 1000, 3040, 9));
    ASSERT(!vol_write_in_range(2048, 1000, 2048, 1001));
    ASSERT(!vol_write_in_range(2048, 1000, 3000, 0xFFFFFFFFFFFFFFFFull));
    PASS();
}

TEST write_empty_or_zero_span_refused(void)
{
    ASSERT(!vol_write_in_range(2048, 1000, 2048, 0));
    ASSERT(!vol_write_in_range(2048, 0, 2048, 1));
    PASS();
}

TEST wbfs_name_case_insensitive(void)
{
    ASSERT(vol_is_wbfs_name("wbfs"));
    ASSERT(vol_is_wbfs_name("WBFS"));
    ASSERT(vol_is_wbfs_name("WbFs"));
    ASSERT(!vol_is_wbfs_name("wbfs2"));
    ASSERT(!vol_is_wbfs_name("wbf"));
    ASSERT(!vol_is_wbfs_name(""));
    ASSERT(!vol_is_wbfs_name("games"));
    PASS();
}

TEST writes_only_on_root_without_wbfs(void)
{
    ASSERT(vol_write_allowed(true, false));
    ASSERT(!vol_write_allowed(true, true));
    ASSERT(!vol_write_allowed(false, false));
    ASSERT(!vol_write_allowed(false, true));
    PASS();
}

SUITE(volume_suite)
{
    RUN_TEST(pick_flash_drive_after_games_drive);
    RUN_TEST(pick_skips_fat_partition_without_root);
    RUN_TEST(pick_sd_without_usb_root);
    RUN_TEST(pick_prefers_usb_over_earlier_sd);
    RUN_TEST(pick_nothing);
    RUN_TEST(pick_ignores_root_flag_on_non_fat);
    RUN_TEST(boot_sector_fat32_and_fat16);
    RUN_TEST(boot_sector_rejects_wbfs_ntfs_exfat);
    RUN_TEST(mbr_lists_primary_entries);
    RUN_TEST(ebr_gives_logical_and_next);
    RUN_TEST(boot_sector_counts_total_sectors);
    RUN_TEST(write_span_is_smaller_of_entry_and_boot);
    RUN_TEST(write_inside_partition_allowed);
    RUN_TEST(write_at_partition_boundary);
    RUN_TEST(write_before_partition_refused);
    RUN_TEST(write_past_partition_end_refused);
    RUN_TEST(write_straddling_end_refused);
    RUN_TEST(write_empty_or_zero_span_refused);
    RUN_TEST(wbfs_name_case_insensitive);
    RUN_TEST(writes_only_on_root_without_wbfs);
}
