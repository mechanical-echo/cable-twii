#include <gccore.h>
#include <fat.h>
#include <ogc/machine/processor.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dol.h"

#define LOAD_ADDR ((u8 *)0x92000000)
#define LOAD_LIMIT ((u8 *)0x933e0000)
#define LOWEST_ADDR 0x80003400u
#define FORWARDER_BASE 0x81230000u
#define STUB_MAGIC ((void *)0x80001804)
#define STUB_MAGIC_LEN 8
#define USB_WAIT_S 10
#define MESSAGE_S 5

typedef void (*dol_entry)(void);

static void screen_init(void)
{
    VIDEO_Init();
    GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
    void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    CON_Init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

static void leave(const char *msg)
{
    printf("\n\n\n   cable-twii\n\n   %s\n\n   Returning to the Wii Menu.\n", msg);
    sleep(MESSAGE_S);
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
    exit(0);
}

static bool mount_usb(void)
{
    for (int i = 0; i < USB_WAIT_S * 5; i++) {
        if (__io_usbstorage.startup() && __io_usbstorage.isInserted()) return fatMountSimple("usb", &__io_usbstorage);
        usleep(200000);
    }
    return false;
}

static FILE *open_app(void)
{
    if (__io_wiisd.startup() && __io_wiisd.isInserted() && fatMountSimple("sd", &__io_wiisd)) {
        FILE *f = fopen("sd:/" APP_PATH, "rb");
        if (f) return f;
        fatUnmount("sd:");
    }
    if (mount_usb()) {
        FILE *f = fopen("usb:/" APP_PATH, "rb");
        if (f) return f;
        fatUnmount("usb:");
    }
    return NULL;
}

static void place(const dol_header *h, const u8 *image)
{
    for (int i = 0; i < DOL_TEXT_SECTIONS; i++) {
        const dol_section *s = &h->text[i];
        if (!s->size) continue;
        memmove((void *)s->address, image + s->offset, s->size);
        DCFlushRange((void *)s->address, s->size);
        ICInvalidateRange((void *)s->address, s->size);
    }
    for (int i = 0; i < DOL_DATA_SECTIONS; i++) {
        const dol_section *s = &h->data[i];
        if (!s->size) continue;
        memmove((void *)s->address, image + s->offset, s->size);
        DCFlushRange((void *)s->address, s->size);
    }
    if (h->bss_size) {
        memset((void *)h->bss_address, 0, h->bss_size);
        DCFlushRange((void *)h->bss_address, h->bss_size);
    }
}

int main(void)
{
    screen_init();
    FILE *f = open_app();
    if (!f) leave(APP_PATH " was not found on the SD card or a USB drive.");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < DOL_HEADER_SIZE || LOAD_ADDR + size > LOAD_LIMIT) {
        fclose(f);
        leave("boot.dol has an unexpected size.");
    }
    if (fread(LOAD_ADDR, 1, size, f) != (size_t)size) {
        fclose(f);
        leave("boot.dol could not be read.");
    }
    fclose(f);
    fatUnmount("sd:");
    fatUnmount("usb:");
    __io_wiisd.shutdown();
    __io_usbstorage.shutdown();
    dol_header h;
    if (!dol_parse(LOAD_ADDR, (size_t)size, &h) || !dol_fits(&h, (size_t)size, LOWEST_ADDR, FORWARDER_BASE)) {
        leave("boot.dol is not a Wii program this channel can start.");
    }
    memset(STUB_MAGIC, 0, STUB_MAGIC_LEN);
    DCFlushRange(STUB_MAGIC, STUB_MAGIC_LEN);
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    place(&h, LOAD_ADDR);
    SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
    u32 level;
    _CPU_ISR_Disable(level);
    ((dol_entry)h.entry)();
    _CPU_ISR_Restore(level);
    return 0;
}
