#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <gccore.h>
#include <fat.h>
#include <ogc/usb.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>

#define MAX_DEVS 4
#define MAX_LUNS 4
#define MAX_USB_DISKS 8
#define MAX_DISKS (MAX_USB_DISKS + 1)
#define MAX_PARTS 16
#define MAX_LOGICAL 16
#define CHUNK_BYTES 65536
#define CACHE_PAGES 8
#define SECTORS_PER_PAGE 64
#define MASS_STORAGE_CLASS 8
#define GPT_PROTECTIVE 0xEE

typedef struct {
    usbstorage_handle handle;
    bool opened;
} usb_dev;

typedef struct {
    vol_source source;
    int dev;
    u8 lun;
    u32 sector_size;
    u32 sectors;
    DISC_INTERFACE iface;
} disk;

typedef struct {
    int disk;
    u32 start;
    bool mounted;
    bool has_root;
    char name[16];
} part;

static usb_dev devs[MAX_DEVS];
static int dev_count;
static disk disks[MAX_DISKS];
static int disk_count;
static part parts[MAX_PARTS];
static int part_count;
static bool sd_started;

static bool usb_read(int d, sec_t sector, sec_t count, void *buffer)
{
    disk *k = &disks[d];
    u8 *out = (u8 *)buffer;
    u32 per = CHUNK_BYTES / k->sector_size;
    while (count > 0) {
        u32 n = count > per ? per : count;
        if (USBStorage_Read(&devs[k->dev].handle, k->lun, sector, (u16)n, out) < 0) return false;
        sector += n;
        count -= n;
        out += n * k->sector_size;
    }
    return true;
}

#define USB_READ_FN(N) static bool usb_read_##N(sec_t s, sec_t c, void *b) { return usb_read(N, s, c, b); }

USB_READ_FN(0)
USB_READ_FN(1)
USB_READ_FN(2)
USB_READ_FN(3)
USB_READ_FN(4)
USB_READ_FN(5)
USB_READ_FN(6)
USB_READ_FN(7)

static const FN_MEDIUM_READSECTORS usb_read_fns[MAX_USB_DISKS] = {
    usb_read_0, usb_read_1, usb_read_2, usb_read_3,
    usb_read_4, usb_read_5, usb_read_6, usb_read_7
};

static bool refuse_write(sec_t sector, sec_t count, const void *buffer)
{
    (void)sector;
    (void)count;
    (void)buffer;
    return false;
}

static bool io_ok(void)
{
    return true;
}

static bool sd_startup(void)
{
    if (!sd_started) sd_started = __io_wiisd.startup();
    return sd_started;
}

static bool sd_inserted(void)
{
    return __io_wiisd.isInserted();
}

static bool sd_read(sec_t sector, sec_t count, void *buffer)
{
    return __io_wiisd.readSectors(sector, count, buffer);
}

static bool sd_clear(void)
{
    return __io_wiisd.clearStatus();
}

static void open_usb(void)
{
    usb_device_entry list[MAX_DEVS];
    u8 cnt = 0;

    USBStorage_Initialize();
    if (USB_GetDeviceList(list, MAX_DEVS, MASS_STORAGE_CLASS, &cnt) < 0) return;

    for (int i = 0; i < cnt && i < MAX_DEVS; i++) {
        usb_dev *dv = &devs[dev_count];
        if (USBStorage_Open(&dv->handle, list[i].device_id, list[i].vid, list[i].pid) < 0) continue;
        dv->opened = true;
        dev_count++;

        s32 luns = USBStorage_GetMaxLUN(&dv->handle);
        if (luns < 1) luns = 1;
        if (luns > MAX_LUNS) luns = MAX_LUNS;
        for (int lun = 0; lun < luns && disk_count < MAX_USB_DISKS; lun++) {
            u32 sector_size = 0, sectors = 0;
            if (USBStorage_MountLUN(&dv->handle, (u8)lun) < 0) continue;
            if (USBStorage_ReadCapacity(&dv->handle, (u8)lun, &sector_size, &sectors) < 0) continue;
            if (sector_size < 512 || sector_size > CHUNK_BYTES) continue;

            disk *k = &disks[disk_count];
            memset(k, 0, sizeof *k);
            k->source = VOL_USB;
            k->dev = dev_count - 1;
            k->lun = (u8)lun;
            k->sector_size = sector_size;
            k->sectors = sectors;
            k->iface.ioType = DEVICE_TYPE_WII_USB;
            k->iface.features = FEATURE_MEDIUM_CANREAD | FEATURE_WII_USB;
            k->iface.startup = io_ok;
            k->iface.isInserted = io_ok;
            k->iface.readSectors = usb_read_fns[disk_count];
            k->iface.writeSectors = refuse_write;
            k->iface.clearStatus = io_ok;
            k->iface.shutdown = io_ok;
            disk_count++;
        }
    }
}

static void open_sd(void)
{
    if (!sd_startup() || !sd_inserted()) return;
    disk *k = &disks[disk_count++];
    memset(k, 0, sizeof *k);
    k->source = VOL_SD;
    k->dev = -1;
    k->sector_size = 512;
    k->iface.ioType = DEVICE_TYPE_WII_SD;
    k->iface.features = FEATURE_MEDIUM_CANREAD | FEATURE_WII_SD;
    k->iface.startup = sd_startup;
    k->iface.isInserted = sd_inserted;
    k->iface.readSectors = sd_read;
    k->iface.writeSectors = refuse_write;
    k->iface.clearStatus = sd_clear;
    k->iface.shutdown = io_ok;
}

static bool read_sector(int d, u32 lba, u8 *buf)
{
    if (disks[d].sectors != 0 && lba >= disks[d].sectors) return false;
    return disks[d].iface.readSectors(lba, 1, buf);
}

static void add_part(int d, u32 start)
{
    if (part_count >= MAX_PARTS) return;
    part *p = &parts[part_count++];
    memset(p, 0, sizeof *p);
    p->disk = d;
    p->start = start;
}

static void probe(int d, u32 start, u8 *buf)
{
    if (read_sector(d, start, buf) && vol_boot_is_fat(buf)) add_part(d, start);
}

static void scan_extended(int d, u32 ext_start, u8 *buf)
{
    u32 ebr = ext_start;
    for (int i = 0; i < MAX_LOGICAL && ebr != 0; i++) {
        vol_part_entry logical;
        u32 next = 0;
        if (!read_sector(d, ebr, buf)) return;
        if (!vol_ebr_parse(buf, ext_start, ebr, &logical, &next)) return;
        if (logical.type != 0 && logical.sectors != 0 && !vol_is_extended(logical.type)) probe(d, logical.start, buf);
        if (next <= ebr) return;
        ebr = next;
    }
}

static void scan_disk(int d, u8 *buf)
{
    if (!read_sector(d, 0, buf)) return;
    if (vol_boot_is_fat(buf)) {
        add_part(d, 0);
        return;
    }
    if (!vol_mbr_valid(buf)) return;

    vol_part_entry entries[4];
    size_t n = vol_mbr_entries(buf, entries);
    for (size_t i = 0; i < n; i++) {
        if (entries[i].type == GPT_PROTECTIVE) continue;
        if (vol_is_extended(entries[i].type)) scan_extended(d, entries[i].start, buf);
        else probe(d, entries[i].start, buf);
    }
}

static void mount_parts(void)
{
    int usb_n = 0, sd_n = 0;
    for (int i = 0; i < part_count; i++) {
        part *p = &parts[i];
        disk *k = &disks[p->disk];
        if (k->source == VOL_SD) {
            if (sd_n == 0) snprintf(p->name, sizeof p->name, "sd");
            else snprintf(p->name, sizeof p->name, "sd%d", sd_n);
            sd_n++;
        } else {
            snprintf(p->name, sizeof p->name, "usb%d", usb_n++);
        }
        if (!fatMount(p->name, &k->iface, p->start, CACHE_PAGES, SECTORS_PER_PAGE)) continue;
        p->mounted = true;

        char path[32];
        struct stat st;
        snprintf(path, sizeof path, "%s:/cable-twii", p->name);
        p->has_root = stat(path, &st) == 0 && S_ISDIR(st.st_mode);
    }
}

static void release_unchosen(int chosen)
{
    int keep_dev = -1;
    bool keep_sd = false;
    if (chosen >= 0) {
        const disk *k = &disks[parts[chosen].disk];
        if (k->source == VOL_USB) keep_dev = k->dev;
        else keep_sd = true;
    }

    for (int i = 0; i < part_count; i++) {
        if (i == chosen || !parts[i].mounted) continue;
        fatUnmount(parts[i].name);
        parts[i].mounted = false;
    }
    for (int i = 0; i < dev_count; i++) {
        if (i == keep_dev || !devs[i].opened) continue;
        USBStorage_Close(&devs[i].handle);
        devs[i].opened = false;
    }
    if (keep_dev < 0) USBStorage_Deinitialize();
    if (!keep_sd && sd_started) {
        __io_wiisd.shutdown();
        sd_started = false;
    }
}

int st_find_root(char *out, size_t out_size, st_volume *vol)
{
    memset(vol, 0, sizeof *vol);
    dev_count = 0;
    disk_count = 0;
    part_count = 0;

    open_usb();
    open_sd();

    u8 *buf = (u8 *)memalign(32, CHUNK_BYTES);
    if (buf != NULL) {
        for (int d = 0; d < disk_count; d++) scan_disk(d, buf);
        free(buf);
    }
    mount_parts();

    vol_candidate candidates[MAX_PARTS];
    for (int i = 0; i < part_count; i++) {
        candidates[i].source = disks[parts[i].disk].source;
        candidates[i].fat = parts[i].mounted;
        candidates[i].has_root = parts[i].has_root;
    }
    int chosen = vol_pick_root(candidates, (size_t)part_count);

    if (chosen >= 0) {
        int len = snprintf(out, out_size, "%s:/cable-twii", parts[chosen].name);
        if (len < 0 || (size_t)len >= out_size) chosen = -1;
    }
    release_unchosen(chosen);
    if (chosen < 0) return -1;

    vol->found = true;
    vol->source = disks[parts[chosen].disk].source;
    snprintf(vol->name, sizeof vol->name, "%s", parts[chosen].name);
    vol->disk = parts[chosen].disk;
    vol->start_sector = parts[chosen].start;
    return 0;
}
