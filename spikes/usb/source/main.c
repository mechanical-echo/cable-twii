#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/usb.h>
#include <ogc/usbstorage.h>

#define MAX_DEVS 4
#define MAX_UNITS 8
#define MAX_PARTS 6
#define CHUNK_BYTES 65536
#define TEST_BYTES (50u * 1024u * 1024u)
#define MIB 1048576.0

typedef struct {
	usbstorage_handle handle;
	usb_device_entry entry;
	bool opened;
} Dev;

typedef struct {
	int dev;
	u8 lun;
	u32 sector_size;
	u32 sectors;
	bool ready;
	u32 write_refusals;
	DISC_INTERFACE iface;
} Unit;

typedef struct {
	int unit;
	int index;
	u8 type;
	u32 start;
	u32 sectors;
	char fs[12];
	bool mounted;
	bool has_dir;
	char mount[12];
} Part;

static Dev devs[MAX_DEVS];
static Unit units[MAX_UNITS];
static Part parts[MAX_PARTS * MAX_UNITS];
static int dev_count;
static int unit_count;
static int part_count;
static void *xfb;
static GXRModeObj *rmode;
static volatile bool want_exit;

static bool unit_read(int u, sec_t sector, sec_t count, void *buffer)
{
	Unit *unit = &units[u];
	u8 *out = buffer;
	u32 per = CHUNK_BYTES / unit->sector_size;

	while (count > 0) {
		u32 n = count > per ? per : count;
		if (USBStorage_Read(&devs[unit->dev].handle, unit->lun, sector, (u16)n, out) < 0) {
			return false;
		}
		sector += n;
		count -= n;
		out += n * unit->sector_size;
	}
	return true;
}

static bool io_noop(void)
{
	return true;
}

#define UNIT_IO(N) \
	static bool unit_read_##N(sec_t s, sec_t c, void *b) { return unit_read(N, s, c, b); } \
	static bool unit_write_##N(sec_t s, sec_t c, const void *b) { (void)s; (void)c; (void)b; units[N].write_refusals++; return false; }

UNIT_IO(0)
UNIT_IO(1)
UNIT_IO(2)
UNIT_IO(3)
UNIT_IO(4)
UNIT_IO(5)
UNIT_IO(6)
UNIT_IO(7)

static FN_MEDIUM_READSECTORS read_fns[MAX_UNITS] = {
	unit_read_0, unit_read_1, unit_read_2, unit_read_3,
	unit_read_4, unit_read_5, unit_read_6, unit_read_7
};

static FN_MEDIUM_WRITESECTORS write_fns[MAX_UNITS] = {
	unit_write_0, unit_write_1, unit_write_2, unit_write_3,
	unit_write_4, unit_write_5, unit_write_6, unit_write_7
};

static u32 le32(const u8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

static u16 le16(const u8 *p)
{
	return p[0] | (p[1] << 8);
}

static void classify(const u8 *boot, char *fs, size_t fs_len)
{
	if (memcmp(boot, "WBFS", 4) == 0) {
		snprintf(fs, fs_len, "WBFS");
	} else if (memcmp(boot + 3, "NTFS    ", 8) == 0) {
		snprintf(fs, fs_len, "NTFS");
	} else if (memcmp(boot + 3, "EXFAT   ", 8) == 0) {
		snprintf(fs, fs_len, "exFAT");
	} else if (boot[510] == 0x55 && boot[511] == 0xAA && memcmp(boot + 0x52, "FAT32   ", 8) == 0
		&& le16(boot + 11) >= 512) {
		snprintf(fs, fs_len, "FAT32");
	} else if (boot[510] == 0x55 && boot[511] == 0xAA && memcmp(boot + 0x36, "FAT16   ", 8) == 0
		&& le16(boot + 11) >= 512) {
		snprintf(fs, fs_len, "FAT16");
	} else if (boot[510] == 0x55 && boot[511] == 0xAA && memcmp(boot + 0x36, "FAT12   ", 8) == 0) {
		snprintf(fs, fs_len, "FAT12");
	} else {
		snprintf(fs, fs_len, "unknown");
	}
}

static bool is_mountable(const char *fs)
{
	return strcmp(fs, "FAT32") == 0 || strcmp(fs, "FAT16") == 0;
}

static void add_part(int u, int index, u8 type, u32 start, u32 sectors, const char *fs)
{
	Part *p;

	if (part_count >= (int)(sizeof(parts) / sizeof(parts[0]))) {
		return;
	}
	p = &parts[part_count++];
	memset(p, 0, sizeof(*p));
	p->unit = u;
	p->index = index;
	p->type = type;
	p->start = start;
	p->sectors = sectors;
	snprintf(p->fs, sizeof(p->fs), "%s", fs);
}

static void scan_unit(int u, u8 *sector0, u8 *scratch)
{
	Unit *unit = &units[u];
	char fs[12];
	bool mbr_valid;
	bool any = false;
	int i;

	classify(sector0, fs, sizeof(fs));
	mbr_valid = sector0[510] == 0x55 && sector0[511] == 0xAA;

	if (strcmp(fs, "unknown") != 0) {
		add_part(u, 0, 0, 0, unit->sectors, fs);
		return;
	}
	if (!mbr_valid) {
		add_part(u, 0, 0, 0, unit->sectors, "unknown");
		return;
	}
	for (i = 0; i < 4; i++) {
		const u8 *e = sector0 + 446 + i * 16;
		u8 type = e[4];
		u32 start = le32(e + 8);
		u32 count = le32(e + 12);

		if (type == 0 || count == 0) {
			continue;
		}
		any = true;
		if (type == 0xEE) {
			add_part(u, i + 1, type, start, count, "GPT");
			continue;
		}
		if (type == 0x05 || type == 0x0F || type == 0x85) {
			add_part(u, i + 1, type, start, count, "extended");
			continue;
		}
		if (start >= unit->sectors || !unit_read(u, start, 1, scratch)) {
			add_part(u, i + 1, type, start, count, "unreadable");
			continue;
		}
		classify(scratch, fs, sizeof(fs));
		add_part(u, i + 1, type, start, count, fs);
	}
	if (!any) {
		add_part(u, 0, 0, 0, unit->sectors, "empty");
	}
}

static void close_all(void)
{
	int i;

	for (i = 0; i < part_count; i++) {
		if (parts[i].mounted) {
			fatUnmount(parts[i].mount);
			parts[i].mounted = false;
		}
	}
	for (i = 0; i < dev_count; i++) {
		if (devs[i].opened) {
			USBStorage_Close(&devs[i].handle);
			devs[i].opened = false;
		}
	}
	USBStorage_Deinitialize();
	USB_Deinitialize();
}

static void print_list_diag(void)
{
	usb_device_entry list[16];
	u8 cnt = 0;
	s32 r;
	int i;

	r = USB_GetDeviceList(list, 16, 0, &cnt);
	printf("all USB devices (class 0): r=%d n=%u\n", (int)r, cnt);
	for (i = 0; i < cnt && i < 16; i++) {
		printf("  id=%08X vid=%04X pid=%04X\n", (unsigned)list[i].device_id, list[i].vid, list[i].pid);
	}
}

static void enumerate(void)
{
	usb_device_entry list[MAX_DEVS];
	u8 cnt = 0;
	s32 r;
	int i;

	dev_count = 0;
	unit_count = 0;
	part_count = 0;
	memset(devs, 0, sizeof(devs));
	memset(units, 0, sizeof(units));
	memset(parts, 0, sizeof(parts));

	r = USBStorage_Initialize();
	printf("USBStorage_Initialize: %d\n", (int)r);
	r = USB_GetDeviceList(list, MAX_DEVS, 8, &cnt);
	printf("mass storage devices (class 8): r=%d n=%u\n", (int)r, cnt);
	if (r < 0 || cnt == 0) {
		print_list_diag();
		return;
	}
	for (i = 0; i < cnt && i < MAX_DEVS; i++) {
		Dev *d = &devs[dev_count];
		s32 lun_count;
		int lun;

		d->entry = list[i];
		r = USBStorage_Open(&d->handle, list[i].device_id, list[i].vid, list[i].pid);
		printf("dev%d id=%08X %04X:%04X open=%d\n", i, (unsigned)list[i].device_id,
			list[i].vid, list[i].pid, (int)r);
		if (r < 0) {
			continue;
		}
		d->opened = true;
		dev_count++;
		lun_count = USBStorage_GetMaxLUN(&d->handle);
		if (lun_count < 1) {
			lun_count = 1;
		}
		if (lun_count > 4) {
			lun_count = 4;
		}
		for (lun = 0; lun < lun_count && unit_count < MAX_UNITS; lun++) {
			Unit *unit = &units[unit_count];
			u32 ssz = 0;
			u32 nsec = 0;

			r = USBStorage_MountLUN(&d->handle, (u8)lun);
			if (r < 0) {
				printf("  lun%d mount=%d\n", lun, (int)r);
				continue;
			}
			r = USBStorage_ReadCapacity(&d->handle, (u8)lun, &ssz, &nsec);
			if (r < 0 || ssz == 0 || ssz > CHUNK_BYTES) {
				printf("  lun%d capacity=%d ssz=%u\n", lun, (int)r, (unsigned)ssz);
				continue;
			}
			unit->dev = dev_count - 1;
			unit->lun = (u8)lun;
			unit->sector_size = ssz;
			unit->sectors = nsec;
			unit->ready = true;
			unit->iface.ioType = DEVICE_TYPE_WII_USB;
			unit->iface.features = FEATURE_MEDIUM_CANREAD | FEATURE_WII_USB;
			unit->iface.startup = io_noop;
			unit->iface.isInserted = io_noop;
			unit->iface.readSectors = read_fns[unit_count];
			unit->iface.writeSectors = write_fns[unit_count];
			unit->iface.clearStatus = io_noop;
			unit->iface.shutdown = io_noop;
			unit_count++;
		}
	}
}

static void scan_all(void)
{
	u8 *sector0 = memalign(32, CHUNK_BYTES);
	u8 *scratch = memalign(32, CHUNK_BYTES);
	int u;

	if (!sector0 || !scratch) {
		printf("out of memory\n");
		free(sector0);
		free(scratch);
		return;
	}
	for (u = 0; u < unit_count; u++) {
		if (!unit_read(u, 0, 1, sector0)) {
			printf("unit%d: sector 0 read failed\n", u);
			continue;
		}
		scan_unit(u, sector0, scratch);
	}
	free(sector0);
	free(scratch);
}

static void mount_and_probe(void)
{
	int i;

	for (i = 0; i < part_count; i++) {
		Part *p = &parts[i];
		struct stat st;
		char path[40];

		if (!is_mountable(p->fs)) {
			continue;
		}
		snprintf(p->mount, sizeof(p->mount), "m%d", i);
		if (!fatMount(p->mount, &units[p->unit].iface, p->start, 4, 64)) {
			continue;
		}
		p->mounted = true;
		snprintf(path, sizeof(path), "%s:/cable-twii", p->mount);
		p->has_dir = stat(path, &st) == 0 && S_ISDIR(st.st_mode);
	}
}

static void print_table(void)
{
	int i;

	printf("\nunits=%d partitions=%d\n", unit_count, part_count);
	for (i = 0; i < unit_count; i++) {
		printf("unit%d dev%d lun%u %.1f GB sec=%u %04X:%04X\n", i, units[i].dev, units[i].lun,
			(double)units[i].sectors * units[i].sector_size / 1e9, (unsigned)units[i].sector_size,
			devs[units[i].dev].entry.vid, devs[units[i].dev].entry.pid);
	}
	for (i = 0; i < part_count; i++) {
		Part *p = &parts[i];

		printf("u%d p%d type=%02X %6.1f GB %-8s", p->unit, p->index, p->type,
			(double)p->sectors * units[p->unit].sector_size / 1e9, p->fs);
		if (is_mountable(p->fs)) {
			printf(" mnt=%s cable-twii=%s", p->mounted ? "ok" : "FAIL", p->has_dir ? "YES" : "no");
		} else {
			printf(" not mounted");
		}
		printf("\n");
	}
}

static bool largest_file(const char *dir, char *best, size_t best_len, off_t *best_size, int depth)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	bool found = false;

	if (!d) {
		return false;
	}
	while ((e = readdir(d)) != NULL) {
		char path[512];
		struct stat st;

		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (stat(path, &st) != 0) {
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			if (depth > 0 && largest_file(path, best, best_len, best_size, depth - 1)) {
				found = true;
			}
		} else if (st.st_size > *best_size) {
			snprintf(best, best_len, "%s", path);
			*best_size = st.st_size;
			found = true;
		}
	}
	closedir(d);
	return found;
}

static void speed_test(Part *p)
{
	char dir[24];
	char path[512];
	off_t size = 0;
	FILE *f;
	u8 *buf;
	u64 start;
	u64 total = 0;
	u64 next_report = 10u * 1024u * 1024u;
	double secs;

	snprintf(dir, sizeof(dir), "%s:/cable-twii", p->mount);
	path[0] = 0;
	if (!largest_file(dir, path, sizeof(path), &size, 2) || size == 0) {
		printf("u%d p%d: no files under /cable-twii\n", p->unit, p->index);
		return;
	}
	f = fopen(path, "rb");
	buf = memalign(32, CHUNK_BYTES);
	if (!f || !buf) {
		printf("u%d p%d: open failed\n", p->unit, p->index);
		if (f) {
			fclose(f);
		}
		free(buf);
		return;
	}
	printf("u%d p%d: %s (%.1f MB)\n", p->unit, p->index, path + 3, size / MIB);
	start = gettime();
	while (total < TEST_BYTES) {
		size_t n = fread(buf, 1, CHUNK_BYTES, f);

		if (n == 0) {
			if (fseek(f, 0, SEEK_SET) != 0) {
				break;
			}
			continue;
		}
		total += n;
		if (total >= next_report) {
			printf("  %u MB\n", (unsigned)(total / 1048576u));
			next_report += 10u * 1024u * 1024u;
		}
	}
	secs = ticks_to_microsecs(gettime() - start) / 1e6;
	fclose(f);
	free(buf);
	printf("u%d p%d: READ %.1f MB in %.1f s = %.2f MB/s\n", p->unit, p->index, total / MIB, secs,
		secs > 0 ? (total / MIB) / secs : 0.0);
}

static void run_scan(void)
{
	int i;
	u32 refusals = 0;
	bool tested = false;

	printf("\x1b[2J\x1b[1;0H");
	printf("cable-twii USB spike (READ ONLY)\n");
	enumerate();
	scan_all();
	mount_and_probe();
	print_table();
	for (i = 0; i < part_count; i++) {
		if (parts[i].mounted && parts[i].has_dir) {
			speed_test(&parts[i]);
			tested = true;
		}
	}
	if (!tested) {
		printf("/cable-twii/ not found on any FAT partition: speed test skipped\n");
	}
	for (i = 0; i < unit_count; i++) {
		refusals += units[i].write_refusals;
	}
	printf("write attempts refused: %u\n", (unsigned)refusals);
	printf("A = rescan   HOME = exit\n");
	close_all();
}

static void on_reset(u32 level, void *arg)
{
	(void)level;
	(void)arg;
	want_exit = true;
}

static void on_power(void)
{
	want_exit = true;
}

static void on_wiimote_power(s32 chan)
{
	(void)chan;
	want_exit = true;
}

int main(void)
{
	IOS_ReloadIOS(58);
	VIDEO_Init();
	WPAD_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	SYS_SetResetCallback(on_reset);
	SYS_SetPowerCallback(on_power);
	WPAD_SetPowerButtonCallback(on_wiimote_power);

	run_scan();
	while (!want_exit) {
		u32 down;

		WPAD_ScanPads();
		down = WPAD_ButtonsDown(0);
		if (down & WPAD_BUTTON_HOME) {
			break;
		}
		if (down & WPAD_BUTTON_A) {
			run_scan();
		}
		VIDEO_WaitVSync();
	}
	exit(0);
	return 0;
}
