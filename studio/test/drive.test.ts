import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { mkdir, mkdtemp, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import {
  defaultRunDiskutil,
  dotClean,
  initRoot,
  inspectDrive,
  scanSource,
  type DiskutilInfo,
  type DriveDeps,
} from "../src/server/drive.js";
import { pickFiles, pickFolder } from "../src/server/picker.js";

let dir: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-drive-"));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

function volumeDeps(info: DiskutilInfo | Error): DriveDeps {
  return {
    isMountPoint: async () => true,
    runDiskutil: async () => {
      if (info instanceof Error) throw info;
      return info;
    },
  };
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
  const info = await inspectDrive(dir, { isMountPoint: async () => false, runDiskutil: async () => FAT });
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
  const info = await inspectDrive(dir, { isMountPoint: async () => false, runDiskutil: async () => FAT });
  expect(info.isVolumeRoot).toBe(false);
  expect(info.fsType).toBeNull();
  expect(info.usable).toBe(true);
});

test("plain folder does not call diskutil", async () => {
  let calls = 0;
  await inspectDrive(dir, {
    isMountPoint: async () => false,
    runDiskutil: async () => {
      calls += 1;
      return FAT;
    },
  });
  expect(calls).toBe(0);
});

test("detects existing root", async () => {
  await mkdir(path.join(dir, "cable-twii"));
  const info = await inspectDrive(dir, { isMountPoint: async () => false, runDiskutil: async () => FAT });
  expect(info.hasRoot).toBe(true);
});

test("treats a path directly under /Volumes as a volume root", async () => {
  const info = await inspectDrive("/Volumes/NoSuchDrive", {
    runDiskutil: async () => ({ FilesystemType: "apfs" }),
  });
  expect(info.isVolumeRoot).toBe(true);
  expect(info.usable).toBe(false);
});

test("refuses system folders and home", async () => {
  const message = "Choose your media drive or a test folder, not a system folder.";
  for (const p of ["/", "/System", "/System/Library", os.homedir()]) {
    const info = await inspectDrive(p, { isMountPoint: async () => false, runDiskutil: async () => FAT });
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
  const info = await inspectDrive(dir, { isMountPoint: async () => false, runDiskutil: async () => FAT });
  await initRoot(info, { isMountPoint: async () => false, runDiskutil: async () => FAT });
  expect(existsSync(path.join(dir, "cable-twii"))).toBe(true);
  expect(existsSync(path.join(dir, "cable-twii", "ads"))).toBe(true);
  expect(existsSync(path.join(dir, "cable-twii", ".studio"))).toBe(true);
});

test("initRoot is idempotent and keeps existing files", async () => {
  const deps: DriveDeps = { isMountPoint: async () => false, runDiskutil: async () => FAT };
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

test("dotClean runs dot_clean -m on a usable volume root", async () => {
  const calls: { cmd: string; args: string[] }[] = [];
  await dotClean(dir, { ...volumeDeps(FAT), run: async (cmd, args) => void calls.push({ cmd, args }) });
  expect(calls).toEqual([{ cmd: "dot_clean", args: ["-m", dir] }]);
});

test("dotClean throws for non volume root, unusable and wbfs", async () => {
  const calls: string[] = [];
  const run = async (cmd: string) => void calls.push(cmd);
  await expect(dotClean(dir, { isMountPoint: async () => false, runDiskutil: async () => FAT, run })).rejects.toThrow();
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
    return "/a/one.mkv\n/a/two.mkv\n";
  });
  expect(calls[0].slice(-1)).toEqual(["Pick files"]);
  expect(calls[0].join(" ")).toContain("choose file with prompt (item 1 of argv) with multiple selections allowed");
  expect(calls[0].join(" ")).not.toContain("Pick files ");
  expect(result).toEqual(["/a/one.mkv", "/a/two.mkv"]);
});

test("pickFiles returns empty list on cancel", async () => {
  const cancel = Object.assign(new Error("x"), { code: 1, stderr: "User canceled. (-128)" });
  expect(await pickFiles("p", async () => Promise.reject(cancel))).toEqual([]);
});
