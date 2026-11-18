import { writeFile } from "node:fs/promises";
import path from "node:path";
import type { convert, probe } from "../src/server/ffmpeg.js";

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

export const fakeProbe: typeof probe = async (file) => {
  const base = path.basename(file);
  if (base.startsWith("broken")) throw new Error("no video stream");
  const durationMs = base.startsWith("short") ? 500 : 60000;
  return { durationMs, width: 640, height: 480, displayAspect: 4 / 3, videoStreamIndex: 0 };
};

export const fakeConvert: typeof convert = async (_src, dst) => {
  await sleep(5);
  await writeFile(dst, "converted");
};

export function multipart(fields: Record<string, string>, file?: { name: string; data: Buffer; type?: string }) {
  const boundary = "----ctwboundary" + Math.random().toString(16).slice(2);
  const chunks: Buffer[] = [];
  for (const [k, v] of Object.entries(fields)) {
    chunks.push(Buffer.from(`--${boundary}\r\nContent-Disposition: form-data; name="${k}"\r\n\r\n${v}\r\n`));
  }
  if (file) {
    chunks.push(Buffer.from(`--${boundary}\r\nContent-Disposition: form-data; name="file"; filename="${file.name}"\r\nContent-Type: ${file.type ?? "application/octet-stream"}\r\n\r\n`));
    chunks.push(file.data, Buffer.from("\r\n"));
  }
  chunks.push(Buffer.from(`--${boundary}--\r\n`));
  return { payload: Buffer.concat(chunks), headers: { "content-type": `multipart/form-data; boundary=${boundary}` } };
}

export async function waitFor(check: () => Promise<boolean>, ms = 3000): Promise<void> {
  const until = Date.now() + ms;
  while (Date.now() < until) {
    if (await check()) return;
    await sleep(20);
  }
  throw new Error("condition not met in time");
}
