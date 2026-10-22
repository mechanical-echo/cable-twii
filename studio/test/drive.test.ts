import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { chmod, mkdir, mkdtemp, realpath, rm, stat, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import {
  defaultBootDevices,
  defaultFindMountPoint,
  defaultRunDiskutil,
  defaultRunVolumeInfo,
  dotClean,
  initRoot,
  inspectDrive,
  isSystemPath,
  scanSource,
  type DiskutilInfo,
  type DriveDeps,
  type RunCommand,
} from "../src/server/drive.js";
import { pickFiles, pickFolder, pickFolderNative, pickFolderWindows } from "../src/server/picker.js";

let dir: string;

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-drive-")));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

function mountAt(mountPoint: string, external = true): DriveDeps["findMountPoint"] {
  return async () => ({ mountPoint, external });
}

function volumeDeps(info: DiskutilInfo | Error, mountPoint: string = dir): DriveDeps {
  return {
    findMountPoint: mountAt(mountPoint),
    runDiskutil: async () => {
      if (info instanceof Error) throw info;
      return info;
    },
  };
}

function plainDeps(): DriveDeps {
  return { findMountPoint: mountAt("/", false), runDiskutil: async () => FAT };
}

const FAT: DiskutilInfo = { FilesystemType: "msdos", FilesystemName: "MS-DOS FAT32" };

function makeVideo(name: string, seconds: number): string {
  const out = path.join(dir, name);
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-f", "lavfi", "-i", `testsrc2=s=64x36:d=${seconds}`, "-f", "lavfi", "-i", `sine=d=${seconds}`, "-shortest", out]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  return out;
}

test("refuses volume with wbfs folder", async () => {
  await mkdir(path.join(dir, "wbfs"));
  const info = await inspectDrive(dir, volumeDeps(FAT));
  expect(info.isVolumeRoot).toBe(true);
  expect(info.hasWbfs).toBe(true);
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("This looks like your games drive (it has a wbfs folder). cable-twii will never write to it.");
});

test("refuses volume with uppercase WBFS folder", async () => {
  await mkdir(path.join(dir, "WBFS"));
  const info = await inspectDrive(dir, volumeDeps(FAT));
  expect(info.hasWbfs).toBe(true);
  expect(info.usable).toBe(false);
});

test("refuses plain folder holding wbfs folder", async () => {
  await mkdir(path.join(dir, "wbfs"));
  const info = await inspectDrive(dir, plainDeps());
  expect(info.isVolumeRoot).toBe(false);
  expect(info.usable).toBe(false);
});

test("refuses drive with a .wbfs file in the second level", async () => {
  await mkdir(path.join(dir, "games"));
  await writeFile(path.join(dir, "games", "Game.wbfs"), "x");
  const info = await inspectDrive(dir, volumeDeps(FAT));
  expect(info.hasWbfs).toBe(true);
  expect(info.usable).toBe(false);
});

test("ignores a .wbfs file in the third level", async () => {
  await mkdir(path.join(dir, "a", "b"), { recursive: true });
  await writeFile(path.join(dir, "a", "b", "Game.wbfs"), "x");
  const info = await inspectDrive(dir, volumeDeps(FAT));
  expect(info.hasWbfs).toBe(false);
  expect(info.usable).toBe(true);
});

test("refuses drive whose filesystem reports wbfs", async () => {
  const info = await inspectDrive(dir, volumeDeps({ FilesystemType: "wbfs", FilesystemName: "WBFS" }));
  expect(info.hasWbfs).toBe(true);
  expect(info.usable).toBe(false);
});

test("refuses non-FAT32 volume", async () => {
  const info = await inspectDrive(dir, volumeDeps({ FilesystemType: "apfs", FilesystemName: "APFS" }));
  expect(info.fsType).toBe("apfs");
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("This drive is apfs. cable-twii needs a FAT32 (MS-DOS) drive.");
});

test("refuses volume when diskutil fails", async () => {
  const info = await inspectDrive(dir, volumeDeps(new Error("boom")));
  expect(info.fsType).toBeNull();
  expect(info.usable).toBe(false);
});

test("accepts FAT32 volume", async () => {
  const info = await inspectDrive(dir, volumeDeps(FAT));
  expect(info.isVolumeRoot).toBe(true);
  expect(info.fsType).toBe("msdos");
  expect(info.usable).toBe(true);
  expect(info.reason).toBeUndefined();
  expect(info.rootPath).toBe(path.join(dir, "cable-twii"));
  expect(info.hasRoot).toBe(false);
  expect(info.freeBytes).toBeGreaterThan(0);
});

test("plain folder allowed for testing", async () => {
  const info = await inspectDrive(dir, plainDeps());
  expect(info.isVolumeRoot).toBe(false);
  expect(info.fsType).toBeNull();
  expect(info.usable).toBe(true);
});

test("plain folder does not call diskutil", async () => {
  let calls = 0;
  await inspectDrive(dir, {
    findMountPoint: mountAt("/", false),
    runDiskutil: async () => {
      calls += 1;
      return FAT;
    },
  });
  expect(calls).toBe(0);
});

test("detects existing root", async () => {
  await mkdir(path.join(dir, "cable-twii"));
  const info = await inspectDrive(dir, plainDeps());
  expect(info.hasRoot).toBe(true);
});

test("refuses a path that does not exist", async () => {
  const info = await inspectDrive("/Volumes/NoSuchDrive/sub", plainDeps());
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("That folder doesn't exist.");
});

test("refuses system folders and home", async () => {
  const message = "Choose your media drive or a test folder, not a system folder.";
  for (const p of ["/", "/System", "/System/Library", "/Library", "/usr", "/Applications", os.homedir()]) {
    const info = await inspectDrive(p, plainDeps());
    expect(info.usable, p).toBe(false);
    expect(info.reason, p).toBe(message);
  }
});

test("default mount detection treats a temp subfolder as plain", async () => {
  const info = await inspectDrive(dir);
  expect(info.isVolumeRoot).toBe(false);
  expect(info.usable).toBe(true);
});

test("default diskutil runner argv is exactly info -plist path", async () => {
  const calls: { cmd: string; args: string[] }[] = [];
  const info = await defaultRunDiskutil("/Volumes/X", async (cmd, args) => {
    calls.push({ cmd, args });
    if (cmd === "diskutil") return "<plist/>";
    return JSON.stringify({ FilesystemType: "msdos", FilesystemName: "MS-DOS FAT32", FreeSpace: 5 });
  });
  expect(calls[0]).toEqual({ cmd: "diskutil", args: ["info", "-plist", "/Volumes/X"] });
  expect(calls.filter((c) => c.cmd === "diskutil")).toHaveLength(1);
  expect(info.FilesystemType).toBe("msdos");
});

test("initRoot creates structure", async () => {
  const info = await inspectDrive(dir, plainDeps());
  await initRoot(info, plainDeps());
  expect(existsSync(path.join(dir, "cable-twii"))).toBe(true);
  expect(existsSync(path.join(dir, "cable-twii", "ads"))).toBe(true);
  expect(existsSync(path.join(dir, "cable-twii", ".studio"))).toBe(true);
});

test("initRoot is idempotent and keeps existing files", async () => {
  const deps = plainDeps();
  await mkdir(path.join(dir, "cable-twii"));
  await writeFile(path.join(dir, "cable-twii", "keep.txt"), "x");
  await initRoot(await inspectDrive(dir, deps), deps);
  expect(existsSync(path.join(dir, "cable-twii", "keep.txt"))).toBe(true);
});

test("initRoot refuses unusable", async () => {
  await mkdir(path.join(dir, "wbfs"));
  const info = await inspectDrive(dir, volumeDeps(FAT));
  await expect(initRoot(info, volumeDeps(FAT))).rejects.toThrow(/games drive/);
  expect(existsSync(path.join(dir, "cable-twii"))).toBe(false);
});

test("initRoot does not trust a forged usable flag", async () => {
  await mkdir(path.join(dir, "wbfs"));
  const real = await inspectDrive(dir, volumeDeps(FAT));
  await expect(initRoot({ ...real, usable: true, hasWbfs: false }, volumeDeps(FAT))).rejects.toThrow();
  expect(existsSync(path.join(dir, "cable-twii"))).toBe(false);
});

test("subfolder of a games drive is refused even when diskutil fails", async () => {
  await mkdir(path.join(dir, "wbfs"));
  await mkdir(path.join(dir, "sub"));
  const info = await inspectDrive(path.join(dir, "sub"), volumeDeps(new Error("boom")));
  expect(info.usable).toBe(false);
  expect(info.hasWbfs).toBe(true);
});

test("subfolder of a games drive is refused even when diskutil says FAT32", async () => {
  await mkdir(path.join(dir, "wbfs"));
  await mkdir(path.join(dir, "sub"));
  const info = await inspectDrive(path.join(dir, "sub"), volumeDeps(FAT));
  expect(info.usable).toBe(false);
  expect(info.reason).toContain("games drive");
});

test("subfolder of an exFAT volume is refused", async () => {
  await mkdir(path.join(dir, "sub"));
  const info = await inspectDrive(path.join(dir, "sub"), volumeDeps({ FilesystemType: "exfat" }));
  expect(info.isVolumeRoot).toBe(false);
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("This drive is exfat. cable-twii needs a FAT32 (MS-DOS) drive.");
});

test("subfolder of a FAT32 volume is refused, root is accepted, diskutil asks the mount point", async () => {
  await mkdir(path.join(dir, "sub"));
  const asked: string[] = [];
  const deps: DriveDeps = {
    findMountPoint: mountAt(dir),
    runDiskutil: async (p) => {
      asked.push(p);
      return FAT;
    },
  };
  const sub = await inspectDrive(path.join(dir, "sub"), deps);
  expect(sub.usable).toBe(false);
  expect(sub.reason).toBe("Pick the drive itself, not a folder inside it.");
  expect(asked).toEqual([dir]);
  expect((await inspectDrive(dir, deps)).usable).toBe(true);
});

test("custom mount point outside /Volumes with wbfs refuses a deep subfolder", async () => {
  const mp = path.join(dir, "mp");
  await mkdir(path.join(mp, "wbfs"), { recursive: true });
  await mkdir(path.join(mp, "apps", "usbloader_gx"), { recursive: true });
  const deps: DriveDeps = {
    statDev: async (p) => (p === mp || p.startsWith(mp + "/") ? 99 : 1),
    bootDevices: async () => [1],
    runDiskutil: async () => FAT,
  };
  const info = await inspectDrive(path.join(mp, "apps", "usbloader_gx"), deps);
  expect(info.usable).toBe(false);
  expect(info.hasWbfs).toBe(true);
  await expect(initRoot(info, deps)).rejects.toThrow(/games drive/);
  expect(existsSync(path.join(mp, "apps", "usbloader_gx", "cable-twii"))).toBe(false);
});

test("custom mount point without wbfs: subfolder refused, root accepted", async () => {
  const mp = path.join(dir, "mp");
  await mkdir(path.join(mp, "apps"), { recursive: true });
  const deps: DriveDeps = {
    statDev: async (p) => (p === mp || p.startsWith(mp + "/") ? 99 : 1),
    bootDevices: async () => [1],
    runDiskutil: async () => FAT,
  };
  const sub = await inspectDrive(path.join(mp, "apps"), deps);
  expect(sub.usable).toBe(false);
  expect(sub.reason).toBe("Pick the drive itself, not a folder inside it.");
  const root = await inspectDrive(mp, deps);
  expect(root.isVolumeRoot).toBe(true);
  expect(root.usable).toBe(true);
});

test("default mount finder: device not in boot set is external", async () => {
  const statDev = async (p: string) => (p === "/a/b" || p === "/a" ? 7 : 1);
  const info = await defaultFindMountPoint("/a/b", statDev, async () => [1]);
  expect(info).toEqual({ mountPoint: "/a", external: true });
});

test("default mount finder: boot device is not external, /Volumes prefix is", async () => {
  const same = async () => 1;
  expect((await defaultFindMountPoint("/x/y", same, async () => [1])).external).toBe(false);
  const vol = async (p: string) => (p.startsWith("/Volumes/X") ? 7 : 1);
  expect((await defaultFindMountPoint("/Volumes/X/sub", vol, async () => [7])).external).toBe(true);
});

test("default boot devices include the Data volume and tolerate its absence", async () => {
  expect(await defaultBootDevices(async (p) => (p === "/" ? 1 : 2))).toEqual([1, 2]);
  expect(
    await defaultBootDevices(async (p) => {
      if (p === "/") return 1;
      throw new Error("nope");
    }),
  ).toEqual([1]);
});

test("refuses /Volumes, /dev and /Users", async () => {
  for (const p of ["/Volumes", "/dev", "/Users"]) {
    const info = await inspectDrive(p, plainDeps());
    expect(info.usable, p).toBe(false);
  }
});

function nestedDeps(work: string, nestedPath: string): DriveDeps {
  return {
    statDev: async (p) => {
      if (p === nestedPath) return 99;
      return (await stat(p)).dev;
    },
    bootDevices: async () => [(await stat(work)).dev],
    runDiskutil: async () => FAT,
  };
}

test("initRoot refuses cable-twii on another device", async () => {
  const work = path.join(dir, "work");
  await mkdir(path.join(work, "cable-twii"), { recursive: true });
  const deps = nestedDeps(work, path.join(work, "cable-twii"));
  const info = await inspectDrive(work, deps);
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("cable-twii is on a different drive");
  await expect(initRoot(info, deps)).rejects.toThrow("cable-twii is on a different drive");
  expect(existsSync(path.join(work, "cable-twii", "ads"))).toBe(false);
  expect(existsSync(path.join(work, "cable-twii", ".studio"))).toBe(false);
});

test("initRoot refuses ads on another device", async () => {
  const work = path.join(dir, "work");
  await mkdir(path.join(work, "cable-twii", "ads"), { recursive: true });
  const deps = nestedDeps(work, path.join(work, "cable-twii", "ads"));
  const info = await inspectDrive(work, deps);
  expect(info.usable).toBe(false);
  await expect(initRoot(info, deps)).rejects.toThrow("cable-twii/ads is on a different drive");
  expect(existsSync(path.join(work, "cable-twii", ".studio"))).toBe(false);
});

test("initRoot refuses .studio on another device", async () => {
  const work = path.join(dir, "work");
  await mkdir(path.join(work, "cable-twii", ".studio"), { recursive: true });
  const deps = nestedDeps(work, path.join(work, "cable-twii", ".studio"));
  const info = await inspectDrive(work, deps);
  expect(info.usable).toBe(false);
  await expect(initRoot(info, deps)).rejects.toThrow("cable-twii/.studio is on a different drive");
});

test("initRoot refuses a folder that lands on another device right after mkdir", async () => {
  const work = path.join(dir, "work");
  await mkdir(work);
  const deps = nestedDeps(work, path.join(work, "cable-twii", "ads"));
  await expect(initRoot(await inspectDrive(work, deps), deps)).rejects.toThrow("cable-twii/ads is on a different drive");
  expect(existsSync(path.join(work, "cable-twii", ".studio"))).toBe(false);
});

test("initRoot checks each level with realpath before going deeper", async () => {
  const deps = plainDeps();
  const work = path.join(dir, "work");
  await mkdir(work);
  await initRoot(await inspectDrive(work, deps), deps);
  expect(await realpath(path.join(work, "cable-twii", "ads"))).toBe(path.join(work, "cable-twii", "ads"));
});

test("symlink earlier in the path to a games drive is refused and nothing is written", async () => {
  const games = path.join(dir, "games");
  await mkdir(path.join(games, "wbfs"), { recursive: true });
  await mkdir(path.join(games, "sub"));
  const other = path.join(dir, "other");
  await mkdir(other);
  await symlink(games, path.join(other, "link"));
  const deps: DriveDeps = {
    findMountPoint: async (p) => ({ mountPoint: p.startsWith(games) ? games : "/", external: p.startsWith(games) }),
    runDiskutil: async () => FAT,
  };
  const info = await inspectDrive(path.join(other, "link", "sub"), deps);
  expect(info.path).toBe(path.join(games, "sub"));
  expect(info.usable).toBe(false);
  await expect(initRoot(info, deps)).rejects.toThrow(/games drive/);
  expect(existsSync(path.join(games, "sub", "cable-twii"))).toBe(false);
});

test("symlinked pick resolves to the real path", async () => {
  await mkdir(path.join(dir, "real"));
  await symlink(path.join(dir, "real"), path.join(dir, "link"));
  const info = await inspectDrive(path.join(dir, "link"), plainDeps());
  expect(info.path).toBe(path.join(dir, "real"));
});

test("wrong-case path cannot dodge the checks", async () => {
  const games = path.join(dir, "Games");
  await mkdir(path.join(games, "wbfs"), { recursive: true });
  await mkdir(path.join(games, "sub"));
  const deps: DriveDeps = {
    findMountPoint: async (p) => ({ mountPoint: p.startsWith(games) ? games : "/", external: p.startsWith(games) }),
    runDiskutil: async () => FAT,
  };
  const swapped = path.join(dir, "gAMES", "sub");
  const info = await inspectDrive(swapped, deps);
  expect(info.usable).toBe(false);
});

test("home reached through a symlink is refused", async () => {
  await symlink(os.homedir(), path.join(dir, "homelink"));
  const info = await inspectDrive(path.join(dir, "homelink"), plainDeps());
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("Choose your media drive or a test folder, not a system folder.");
});

test("system folder reached through a symlink is refused", async () => {
  await symlink("/System/Library", path.join(dir, "syslink"));
  const info = await inspectDrive(path.join(dir, "syslink"), plainDeps());
  expect(info.usable).toBe(false);
});

test("initRoot refuses a symlinked cable-twii and writes nothing through it", async () => {
  const target = path.join(dir, "elsewhere");
  await mkdir(target);
  const work = path.join(dir, "work");
  await mkdir(work);
  await symlink(target, path.join(work, "cable-twii"));
  const deps = plainDeps();
  await expect(initRoot(await inspectDrive(work, deps), deps)).rejects.toThrow("cable-twii is not a normal folder");
  expect(existsSync(path.join(target, "ads"))).toBe(false);
});

test("initRoot refuses ads that is a file", async () => {
  const work = path.join(dir, "work");
  await mkdir(path.join(work, "cable-twii"), { recursive: true });
  await writeFile(path.join(work, "cable-twii", "ads"), "x");
  const deps = plainDeps();
  await expect(initRoot(await inspectDrive(work, deps), deps)).rejects.toThrow("cable-twii/ads is not a normal folder");
  expect(existsSync(path.join(work, "cable-twii", ".studio"))).toBe(false);
});

test("initRoot refuses a symlinked .studio", async () => {
  const target = path.join(dir, "elsewhere");
  await mkdir(target);
  const work = path.join(dir, "work");
  await mkdir(path.join(work, "cable-twii"), { recursive: true });
  await symlink(target, path.join(work, "cable-twii", ".studio"));
  const deps = plainDeps();
  await expect(initRoot(await inspectDrive(work, deps), deps)).rejects.toThrow("cable-twii/.studio is not a normal folder");
});

test("initRoot refuses a pick on a games drive subfolder", async () => {
  await mkdir(path.join(dir, "wbfs"));
  await mkdir(path.join(dir, "sub"));
  const deps = volumeDeps(FAT);
  await expect(initRoot({ ...(await inspectDrive(dir, plainDeps())), path: path.join(dir, "sub") }, deps)).rejects.toThrow(/games drive/);
  expect(existsSync(path.join(dir, "sub", "cable-twii"))).toBe(false);
});

test("scan skips unreadable subfolders", async () => {
  makeVideo("good.mkv", 1);
  const locked = path.join(dir, "locked");
  await mkdir(locked);
  await writeFile(path.join(locked, "hidden.mkv"), "x");
  await chmod(locked, 0o000);
  try {
    const result = await scanSource(dir);
    expect(result.map((r) => path.basename(r.source))).toEqual(["good.mkv"]);
  } finally {
    await chmod(locked, 0o755);
  }
});

test("scan skips dot folders and dot files", async () => {
  makeVideo("good.mkv", 1);
  for (const name of [".Trashes", ".Spotlight-V100", ".hidden"]) {
    await mkdir(path.join(dir, name));
    await writeFile(path.join(dir, name, "x.mkv"), "x");
  }
  await writeFile(path.join(dir, ".dot.mkv"), "x");
  const result = await scanSource(dir);
  expect(result.map((r) => path.basename(r.source))).toEqual(["good.mkv"]);
});

test("dotClean runs dot_clean -m on a usable volume root", async () => {
  const calls: { cmd: string; args: string[] }[] = [];
  await dotClean(dir, { ...volumeDeps(FAT), run: async (cmd, args) => void calls.push({ cmd, args }) });
  expect(calls).toEqual([{ cmd: "dot_clean", args: ["-m", dir] }]);
});

test("dotClean throws for non volume root, unusable and wbfs", async () => {
  const calls: string[] = [];
  const run = async (cmd: string) => void calls.push(cmd);
  await expect(dotClean(dir, { ...plainDeps(), run })).rejects.toThrow();
  await expect(dotClean(dir, { ...volumeDeps({ FilesystemType: "apfs" }), run })).rejects.toThrow();
  await mkdir(path.join(dir, "wbfs"));
  await expect(dotClean(dir, { ...volumeDeps(FAT), run })).rejects.toThrow();
  expect(calls).toEqual([]);
});

test("scan_ignores_non_video", async () => {
  makeVideo("a.mkv", 0.5);
  await writeFile(path.join(dir, "a.srt"), "x");
  await writeFile(path.join(dir, "a.nfo"), "x");
  await writeFile(path.join(dir, "cover.jpg"), "x");
  await writeFile(path.join(dir, "._a.mkv"), "x");
  const result = await scanSource(dir);
  expect(result).toHaveLength(1);
  expect(result[0].source).toBe(path.join(dir, "a.mkv"));
  expect(result[0].tooShort).toBe(true);
  expect(result[0].error).toBeUndefined();
  expect(result[0].aspect).toBe("16:9");
});

test("scan recurses, sorts naturally and puts unknown last", async () => {
  await mkdir(path.join(dir, "sub"));
  const first = makeVideo("Show.S01E10.mkv", 1);
  makeVideo("Show.S01E02.mkv", 1);
  makeVideo("Show.S02E01.mkv", 1);
  makeVideo("random clip.mkv", 1);
  const nested = path.join(dir, "sub", "Show.S01E01.mkv");
  spawnSync("cp", [first, nested]);
  const result = await scanSource(dir);
  expect(result.map((r) => path.basename(r.source))).toEqual([
    "Show.S01E01.mkv",
    "Show.S01E02.mkv",
    "Show.S01E10.mkv",
    "Show.S02E01.mkv",
    "random clip.mkv",
  ]);
});

test("scan reports a bad file without stopping", async () => {
  makeVideo("good.mkv", 1);
  await writeFile(path.join(dir, "bad.mkv"), "not a video");
  const result = await scanSource(dir);
  expect(result).toHaveLength(2);
  const bad = result.find((r) => r.source.endsWith("bad.mkv"))!;
  expect(bad.error).toBeTruthy();
  expect(bad.durationMs).toBe(0);
  expect(bad.aspect).toBe("4:3");
  expect(bad.tooShort).toBe(true);
  expect(result.find((r) => r.source.endsWith("good.mkv"))!.error).toBeUndefined();
});

test("scan does not follow symlinks", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-outside-"));
  try {
    await writeFile(path.join(outside, "x.mkv"), "x");
    await symlink(outside, path.join(dir, "link"));
    await symlink(path.join(outside, "x.mkv"), path.join(dir, "linked.mkv"));
    expect(await scanSource(dir)).toEqual([]);
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});

test("pickFolder passes prompt as argv and strips trailing slash", async () => {
  const calls: string[][] = [];
  const result = await pickFolder('Pick "my" folder; do shell script "x"', async (args) => {
    calls.push(args);
    return "/Users/a/Videos/\n";
  });
  expect(calls[0]).toEqual([
    "-e",
    "on run argv",
    "-e",
    "POSIX path of (choose folder with prompt (item 1 of argv))",
    "-e",
    "end run",
    'Pick "my" folder; do shell script "x"',
  ]);
  expect(result).toBe("/Users/a/Videos");
});

test("pickFolder returns null on cancel", async () => {
  const cancel = Object.assign(new Error("x"), { code: 1, stderr: "execution error: User canceled. (-128)" });
  expect(await pickFolder("p", async () => Promise.reject(cancel))).toBeNull();
});

test("pickFolder rethrows other errors", async () => {
  const other = Object.assign(new Error("boom"), { code: 1, stderr: "something else" });
  await expect(pickFolder("p", async () => Promise.reject(other))).rejects.toThrow("boom");
});

test("pickFiles passes prompt as argv and splits lines", async () => {
  const calls: string[][] = [];
  const result = await pickFiles("Pick files", async (args) => {
    calls.push(args);
    return "/a/one.mkv\u001f/a/tw\no.mkv\n";
  });
  expect(calls[0].slice(-1)).toEqual(["Pick files"]);
  expect(calls[0].join(" ")).toContain("choose file with prompt (item 1 of argv) with multiple selections allowed");
  expect(calls[0].join(" ")).not.toContain("Pick files ");
  expect(result).toEqual(["/a/one.mkv", "/a/tw\no.mkv"]);
});

test("pickFiles returns empty list on cancel", async () => {
  const cancel = Object.assign(new Error("x"), { code: 1, stderr: "User canceled. (-128)" });
  expect(await pickFiles("p", async () => Promise.reject(cancel))).toEqual([]);
});

function windowsDeps(extra: Partial<DriveDeps> = {}): DriveDeps {
  return {
    platform: "win32",
    env: {
      SystemDrive: "C:",
      SystemRoot: "C:\\Windows",
      ProgramFiles: "C:\\Program Files",
      "ProgramFiles(x86)": "C:\\Program Files (x86)",
      ProgramData: "C:\\ProgramData",
    },
    homedir: () => "C:\\Users\\me",
    realpathNative: async (p) => p,
    ...extra,
  };
}

test("windows system folders, profile roots and home are refused", async () => {
  const refused = [
    "C:\\",
    "c:\\",
    "C:\\Windows",
    "c:\\WINDOWS\\system32",
    "C:\\Program Files\\x",
    "C:\\Program Files (x86)",
    "C:\\ProgramData\\y",
    "C:\\Users",
    "c:\\users",
    "C:\\Users\\Public",
    "C:\\Users\\Default",
    "C:\\Users\\other",
    "C:\\Users\\me",
    "c:\\USERS\\ME",
    "C:\\$Recycle.Bin",
    "C:\\$Recycle.Bin\\S-1-5-21",
    "C:\\System Volume Information",
    "C:\\Recovery\\x",
    "C:\\Boot",
    "c:\\boot\\en-US",
  ];
  for (const p of refused) expect(await isSystemPath(p, windowsDeps()), p).toBe(true);
});

test("windows test folders are allowed", async () => {
  const allowed = ["C:\\Users\\me\\Desktop\\ctw-test", "C:\\ctw-test", "C:\\Windowsish", "C:\\Bootcamp", "D:\\Windows", "D:\\Users"];
  for (const p of allowed) expect(await isSystemPath(p, windowsDeps()), p).toBe(false);
});

test("windows system folders are compared against their real paths", async () => {
  const deps = windowsDeps({
    env: { SystemDrive: "D:", SystemRoot: "D:\\Win\\" },
    homedir: () => "D:\\Profiles\\me",
    realpathNative: async (p) => {
      if (p === "D:\\Win\\") return "E:\\RealWin";
      if (p === "D:\\Profiles\\me") return "E:\\Home";
      throw new Error("no such path");
    },
  });
  for (const p of ["E:\\RealWin\\System32", "e:\\home", "D:\\", "D:\\Users", "D:\\Program Files\\x", "D:\\Recovery"]) {
    expect(await isSystemPath(p, deps), p).toBe(true);
  }
  expect(await isSystemPath("C:\\Windows\\x", deps)).toBe(false);
});

test("windows falls back to drive C when SystemDrive is not a drive letter", async () => {
  const deps = windowsDeps({ env: { SystemDrive: "\\\\server\\share" } });
  expect(await isSystemPath("C:\\", deps)).toBe(true);
  expect(await isSystemPath("C:\\Windows", deps)).toBe(true);
});

test("macOS home is refused even when it sits under an allowed temp folder", async () => {
  expect(await isSystemPath(dir, { platform: "darwin", homedir: () => dir })).toBe(true);
  expect(await isSystemPath(path.join(dir, "sub"), { platform: "darwin", homedir: () => dir })).toBe(false);
});

test("inspectDrive uses the injected home", async () => {
  const info = await inspectDrive(dir, { ...plainDeps(), homedir: () => dir });
  expect(info.usable).toBe(false);
  expect(info.reason).toBe("Choose your media drive or a test folder, not a system folder.");
});

test("windows boot device is the system drive root", async () => {
  const asked: string[] = [];
  const statDev = async (p: string) => {
    asked.push(p);
    return 3;
  };
  expect(await defaultBootDevices(statDev, "win32", { SystemDrive: "D:" })).toEqual([3]);
  expect(await defaultBootDevices(statDev, "win32", { SystemDrive: "$(calc)" })).toEqual([3]);
  expect(asked).toEqual(["D:\\", "C:\\"]);
});

test("windows mount finder walks windows paths and uses only the boot set", async () => {
  const statDev = async (p: string) => (p.toUpperCase().startsWith("E:") ? 7 : 1);
  expect(await defaultFindMountPoint("E:\\shows\\x", statDev, async () => [1], "win32")).toEqual({ mountPoint: "E:\\", external: true });
  expect((await defaultFindMountPoint("C:\\Users\\me\\t", async () => 1, async () => [1], "win32")).external).toBe(false);
});

type ExecCall = { cmd: string; args: string[]; env?: Record<string, string> };

function volumeExec(output: string, calls: ExecCall[] = []): RunCommand {
  return async (cmd, args, _input, env) => {
    calls.push({ cmd, args, env });
    return output;
  };
}

test("windows volume info passes the mount point as data, never inside the script", async () => {
  const calls: ExecCall[] = [];
  const info = await defaultRunVolumeInfo("E:\\", volumeExec('{"FileSystem":"FAT32"}', calls));
  expect(info).toEqual({ FilesystemType: "msdos", FilesystemName: "FAT32" });
  expect(calls).toHaveLength(1);
  expect(calls[0].cmd).toBe("powershell.exe");
  expect(calls[0].env).toEqual({ CTW_VOLUME_PATH: "E:\\" });
  const script = calls[0].args.join(" ");
  expect(script).not.toContain("E:");
  expect(script).toContain("Win32_Volume");
  expect(script).toContain("$env:CTW_VOLUME_PATH");
  expect(script).not.toContain("FreeSpace");
});

test("windows volume info asks for a folder-mounted volume by its mount point", async () => {
  const calls: ExecCall[] = [];
  await defaultRunVolumeInfo("C:\\mnt\\usb $(calc)", volumeExec('{"FileSystem":"FAT32"}', calls));
  expect(calls[0].env).toEqual({ CTW_VOLUME_PATH: "C:\\mnt\\usb $(calc)\\" });
  expect(calls[0].args.join(" ")).not.toContain("calc");
});

test("windows volume info reports other filesystems and fails closed on unknown values", async () => {
  expect(await defaultRunVolumeInfo("E:\\", volumeExec('{"FileSystem":"NTFS"}'))).toEqual({ FilesystemType: "ntfs", FilesystemName: "NTFS" });
  expect(await defaultRunVolumeInfo("E:\\", volumeExec('{"FileSystem":"exFAT"}'))).toEqual({ FilesystemType: "exfat", FilesystemName: "exFAT" });
  for (const out of ['{"FileSystem":6}', '{"FileSystemType":6}', '{"FileSystem":null}', '{"FileSystem":""}', "{}", "[]", "null"]) {
    expect(await defaultRunVolumeInfo("E:\\", volumeExec(out)), out).toEqual({});
  }
  await expect(defaultRunVolumeInfo("E:\\", volumeExec("not json"))).rejects.toThrow();
});

test("windows volume info refuses anything but a drive letter path before running PowerShell", async () => {
  const calls: ExecCall[] = [];
  const bad = ["\\\\server\\share\\", "\\\\?\\C:\\", "\\\\.\\C:\\", "\\\\?\\Volume{1}\\", "$(calc):\\", "1:\\", "EE:\\", "E:", "relative\\x", "/Volumes/X"];
  for (const p of bad) await expect(defaultRunVolumeInfo(p, volumeExec('{"FileSystem":"FAT32"}', calls)), p).rejects.toThrow();
  expect(calls).toHaveLength(0);
});

test("windows inspectDrive refuses a volume whose filesystem cannot be read", async () => {
  const calls: ExecCall[] = [];
  const info = await inspectDrive(dir, { platform: "win32", findMountPoint: mountAt(dir), exec: volumeExec('{"FileSystem":"FAT32"}', calls) });
  expect(info.usable).toBe(false);
  expect(info.fsType).toBeNull();
  expect(calls).toHaveLength(0);
});

test("dotClean does nothing on windows", async () => {
  const runs: string[][] = [];
  await dotClean(dir, { ...volumeDeps(FAT), platform: "win32", run: async (_cmd, args) => runs.push(args) });
  expect(runs).toEqual([]);
});

test("windows folder picker forces UTF-8 output and escapes quotes in the prompt", async () => {
  const scripts: string[] = [];
  const result = await pickFolderWindows("Pick 'my' \u2018drive\u2019", async (script) => {
    scripts.push(script);
    return "E:\\Shows \u2728\r\n";
  });
  expect(scripts[0].startsWith("[Console]::OutputEncoding = [System.Text.Encoding]::UTF8;")).toBe(true);
  expect(scripts[0]).toContain("$f.Description = 'Pick ''my'' \u2018\u2018drive\u2019\u2019'");
  expect(result).toBe("E:\\Shows \u2728");
});

test("windows folder picker returns null on cancel", async () => {
  expect(await pickFolderWindows("p", async () => "")).toBeNull();
});

test("native folder picker chooses the runner by platform", async () => {
  const never = async (): Promise<string> => Promise.reject(new Error("wrong runner"));
  expect(await pickFolderNative("p", { platform: "win32", powershell: async () => "E:\\x", osascript: never })).toBe("E:\\x");
  expect(await pickFolderNative("p", { platform: "darwin", powershell: never, osascript: async () => "/Volumes/X/\n" })).toBe("/Volumes/X");
});
