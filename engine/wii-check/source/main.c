#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <fat.h>
#include <gccore.h>
#include <sdcard/wiisd_io.h>
#include <wiiuse/wpad.h>

#include "ctw/ctw.h"

#define CHECK_ROOT "sd:/cable-twii/"
#define UNICODE_ROOT "sd:/cable-twii-unicode/"

static void *xfb;
static GXRModeObj *rmode;

static const ctw_date check_dates[] = {
    {2026, 10, 20},
    {2026, 10, 21},
    {2026, 12, 31},
    {2027, 1, 1},
};

static void out(const char *fmt, ...)
{
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    SYS_Report("%s\n", buf);
}

static void video_init(void)
{
    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    CON_Init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
}

static void report_issues(const ctw_issues *issues)
{
    size_t i;
    for (i = 0; i < issues->count; i++) {
        const ctw_issue *is = &issues->items[i];
        out("%s %s: %s", is->level == CTW_LEVEL_ERROR ? "ERROR" : "WARNING", is->file,
            is->message);
    }
}

static void run_check(const char *root, const char *label)
{
    ctw_db *db = NULL;
    ctw_issues issues = {0};
    size_t d, c;
    if (ctw_db_load(root, &db, &issues)) {
        report_issues(&issues);
        out("ERROR load failed: %s", root);
        ctw_issues_free(&issues);
        ctw_db_free(db);
        return;
    }
    report_issues(&issues);
    for (d = 0; d < sizeof check_dates / sizeof check_dates[0]; d++) {
        ctw_date day = check_dates[d];
        for (c = 0; c < ctw_db_channel_count(db); c++) {
            const ctw_channel *ch = ctw_db_channel_at(db, c);
            ctw_lineup l = {0};
            uint64_t h;
            if (ctw_lineup_build(db, ch, day, &l)) {
                out("ERROR lineup %s%04d-%02d-%02d %d failed", label, day.year, day.month,
                    day.day, ch->number);
                ctw_lineup_free(&l);
                continue;
            }
            h = ctw_lineup_hash(&l);
            out("HASH %s%04d-%02d-%02d %d %08lx%08lx", label, day.year, day.month, day.day,
                ch->number, (unsigned long)(uint32_t)(h >> 32), (unsigned long)(uint32_t)h);
            ctw_lineup_free(&l);
        }
    }
    ctw_issues_free(&issues);
    ctw_db_free(db);
}

#ifndef AUTOTEST
static void wait_for_exit(void)
{
    out("press HOME or START to exit");
    for (;;) {
        WPAD_ScanPads();
        PAD_ScanPads();
        if ((WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0) & PAD_BUTTON_START))
            break;
        VIDEO_WaitVSync();
    }
}
#endif

int main(int argc, char **argv)
{
    int sd_ok;
    (void)argc;
    (void)argv;
    video_init();
    WPAD_Init();
    PAD_Init();
    out("cable-twii wii-check");
    sd_ok = fatMountSimple("sd", &__io_wiisd);
    if (!sd_ok) {
        out("ERROR sd mount failed");
    } else {
        run_check(CHECK_ROOT, "");
        run_check(UNICODE_ROOT, "unicode ");
        fatUnmount("sd");
    }
    out("DONE");
#ifdef AUTOTEST
    WPAD_Shutdown();
    SYS_ResetSystem(SYS_POWEROFF, 0, 0);
#else
    wait_for_exit();
    WPAD_Shutdown();
#endif
    exit(0);
    return 0;
}
