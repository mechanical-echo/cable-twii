#include <string.h>
#include "greatest/greatest.h"
#include "dol.h"

#define LOW 0x80003400u
#define HIGH 0x81230000u
#define IMAGE_SIZE 0x140

static void put32(uint8_t *image, size_t offset, uint32_t value)
{
    image[offset] = (uint8_t)(value >> 24);
    image[offset + 1] = (uint8_t)(value >> 16);
    image[offset + 2] = (uint8_t)(value >> 8);
    image[offset + 3] = (uint8_t)value;
}

static void app_image(uint8_t *image)
{
    memset(image, 0, IMAGE_SIZE);
    put32(image, 0x00, 0x100);
    put32(image, 0x48, 0x80003F00);
    put32(image, 0x90, 0x20);
    put32(image, 0x1C, 0x120);
    put32(image, 0x64, 0x80100000);
    put32(image, 0xAC, 0x10);
    put32(image, 0xD8, 0x80200000);
    put32(image, 0xDC, 0x1000);
    put32(image, 0xE0, 0x80003F00);
}

TEST parse_reads_big_endian_fields(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_EQ_FMT(0x100u, h.text[0].offset, "%x");
    ASSERT_EQ_FMT(0x80003F00u, h.text[0].address, "%x");
    ASSERT_EQ_FMT(0x20u, h.text[0].size, "%x");
    ASSERT_EQ_FMT(0x120u, h.data[0].offset, "%x");
    ASSERT_EQ_FMT(0x80100000u, h.data[0].address, "%x");
    ASSERT_EQ_FMT(0x10u, h.data[0].size, "%x");
    ASSERT_EQ_FMT(0u, h.text[6].size, "%x");
    ASSERT_EQ_FMT(0u, h.data[10].size, "%x");
    ASSERT_EQ_FMT(0x80200000u, h.bss_address, "%x");
    ASSERT_EQ_FMT(0x1000u, h.bss_size, "%x");
    ASSERT_EQ_FMT(0x80003F00u, h.entry, "%x");
    PASS();
}

TEST parse_rejects_short_image(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    ASSERT_FALSE(dol_parse(image, DOL_HEADER_SIZE - 1, &h));
    PASS();
}

TEST fits_accepts_app_inside_the_window(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_rejects_section_past_file_end(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0x90, 0x41);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_FALSE(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_rejects_section_reaching_the_forwarder(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0x64, HIGH - 0x8);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_FALSE(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_rejects_section_below_the_window(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0x48, 0x80001800);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_FALSE(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_rejects_bss_reaching_the_forwarder(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0xD8, 0x81000000);
    put32(image, 0xDC, 0x300000);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_FALSE(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_rejects_entry_outside_the_window(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0xE0, 0x81400000);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_FALSE(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_ignores_empty_sections(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0x68, 0xFFFFFFF0);
    put32(image, 0x20, 0xFFFFFFF0);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

TEST fits_rejects_address_wraparound(void)
{
    uint8_t image[IMAGE_SIZE];
    dol_header h;
    app_image(image);
    put32(image, 0x64, 0xFFFFFFF0);
    ASSERT(dol_parse(image, IMAGE_SIZE, &h));
    ASSERT_FALSE(dol_fits(&h, IMAGE_SIZE, LOW, HIGH));
    PASS();
}

SUITE(dol_suite)
{
    RUN_TEST(parse_reads_big_endian_fields);
    RUN_TEST(parse_rejects_short_image);
    RUN_TEST(fits_accepts_app_inside_the_window);
    RUN_TEST(fits_rejects_section_past_file_end);
    RUN_TEST(fits_rejects_section_reaching_the_forwarder);
    RUN_TEST(fits_rejects_section_below_the_window);
    RUN_TEST(fits_rejects_bss_reaching_the_forwarder);
    RUN_TEST(fits_rejects_entry_outside_the_window);
    RUN_TEST(fits_ignores_empty_sections);
    RUN_TEST(fits_rejects_address_wraparound);
}
