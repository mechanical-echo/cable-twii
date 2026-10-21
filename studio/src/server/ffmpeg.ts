import { spawn } from "node:child_process";
import { rename, rm } from "node:fs/promises";
import {
  AUDIO_BITRATE,
  AUDIO_CHANNELS,
  AUDIO_CODEC,
  AUDIO_SAMPLE_RATE,
  CONTAINER_FORMAT,
  FRAME_RATE,
  FRAME_SIZE,
  GOP_SIZE,
  KILL_GRACE_MS,
  LOUDNORM,
  MIN_DURATION_MS,
  STDERR_TAIL_LINES,
  VIDEO_BITRATE,
  VIDEO_BUFSIZE,
  VIDEO_CODEC,
  VIDEO_MAXRATE,
  WIDE_ASPECT_THRESHOLD,
  type Aspect,
} from "./videoSettings.js";

export { MIN_DURATION_MS };

export interface ProbeResult {
  durationMs: number;
  width: number;
  height: number;
  displayAspect: number;
  videoStreamIndex: number;
}

export interface FfprobeStream {
  index?: number;
  duration?: string;
  disposition?: { attached_pic?: number };
  codec_type?: string;
  width?: number;
  height?: number;
  display_aspect_ratio?: string;
  sample_aspect_ratio?: string;
}

interface FfprobeOutput {
  format?: { duration?: string };
  streams?: FfprobeStream[];
}

function parseRatio(text: string | undefined): number | null {
  if (!text) return null;
  const match = /^(\d+(?:\.\d+)?):(\d+(?:\.\d+)?)$/.exec(text);
  if (!match) return null;
  const num = Number(match[1]);
  const den = Number(match[2]);
  if (num === 0 || den === 0) return null;
  return num / den;
}

export function displayAspectOf(stream: FfprobeStream): number {
  const declared = parseRatio(stream.display_aspect_ratio);
  if (declared !== null) return declared;
  const sar = parseRatio(stream.sample_aspect_ratio) ?? 1;
  return ((stream.width ?? 0) * sar) / (stream.height ?? 1);
}

export function probe(path: string): Promise<ProbeResult> {
  return new Promise((resolve, reject) => {
    const child = spawn("ffprobe", ["-v", "error", "-print_format", "json", "-show_format", "-show_streams", "-i", `file:${path}`]);
    let out = "";
    let err = "";
    child.stdout.on("data", (chunk: Buffer) => {
      out += chunk.toString();
    });
    child.stderr.on("data", (chunk: Buffer) => {
      err += chunk.toString();
    });
    child.on("error", (e) => reject(new Error(`ffprobe failed to start: ${e.message}`)));
    child.on("close", (code) => {
      if (code !== 0) {
        reject(new Error(`ffprobe exited with code ${code}: ${err.trim()}`));
        return;
      }
      let parsed: FfprobeOutput;
      try {
        parsed = JSON.parse(out) as FfprobeOutput;
      } catch {
        reject(new Error("ffprobe returned invalid JSON"));
        return;
      }
      const video = (parsed.streams ?? []).find((s) => s.codec_type === "video" && !s.disposition?.attached_pic);
      if (!video || !video.width || !video.height) {
        reject(new Error(`no video stream found in ${path}`));
        return;
      }
      const formatSeconds = parsed.format?.duration === undefined ? NaN : Number(parsed.format.duration);
      const streamSeconds = video.duration === undefined ? NaN : Number(video.duration);
      const seconds = Number.isFinite(formatSeconds) ? formatSeconds : streamSeconds;
      if (!Number.isFinite(seconds)) {
        reject(new Error(`duration unknown for ${path}`));
        return;
      }
      resolve({ durationMs: Math.round(seconds * 1000), width: video.width, height: video.height, displayAspect: displayAspectOf(video), videoStreamIndex: video.index ?? 0 });
    });
  });
}

export function isTooShort(durationMs: number): boolean {
  return durationMs < MIN_DURATION_MS;
}

export function aspectFor(displayAspect: number): Aspect {
  return displayAspect >= WIDE_ASPECT_THRESHOLD ? "16:9" : "4:3";
}

export function conversionArgs(src: string, dst: string, aspect: Aspect, videoStreamIndex = 0): string[] {
  const { width, height } = FRAME_SIZE[aspect];
  const videoFilter = `scale=${width}:${height}:force_original_aspect_ratio=decrease,pad=${width}:${height}:(ow-iw)/2:(oh-ih)/2,fps=${FRAME_RATE}`;
  return [
    "-y",
    "-nostdin",
    "-progress",
    "pipe:1",
    "-nostats",
    "-i",
    `file:${src}`,
    "-map",
    `0:${videoStreamIndex}`,
    "-map",
    "0:a:0?",
    "-sn",
    "-dn",
    "-vf",
    videoFilter,
    "-c:v",
    VIDEO_CODEC,
    "-b:v",
    VIDEO_BITRATE,
    "-maxrate",
    VIDEO_MAXRATE,
    "-bufsize",
    VIDEO_BUFSIZE,
    "-g",
    String(GOP_SIZE),
    "-af",
    LOUDNORM,
    "-c:a",
    AUDIO_CODEC,
    "-ar",
    String(AUDIO_SAMPLE_RATE),
    "-ac",
    String(AUDIO_CHANNELS),
    "-b:a",
    AUDIO_BITRATE,
    "-f",
    CONTAINER_FORMAT,
    `file:${dst}`,
  ];
}

export async function convert(
  src: string,
  dst: string,
  aspect: Aspect,
  onProgress: (ratio: number) => void,
  signal?: AbortSignal,
): Promise<void> {
  const abortError = () => new Error("conversion aborted");
  if (signal?.aborted) throw abortError();
  const { durationMs, videoStreamIndex } = await probe(src);
  if (signal?.aborted) throw abortError();
  const part = dst + ".part";

  await new Promise<void>((resolve, reject) => {
    const child = spawn("ffmpeg", conversionArgs(src, part, aspect, videoStreamIndex));
    let stdoutBuffer = "";
    let stderrBuffer = "";
    let aborted = false;
    let killTimer: NodeJS.Timeout | undefined;

    const onAbort = () => {
      aborted = true;
      child.kill("SIGTERM");
      killTimer = setTimeout(() => child.kill("SIGKILL"), KILL_GRACE_MS);
    };
    signal?.addEventListener("abort", onAbort, { once: true });

    const handleLine = (line: string) => {
      const eq = line.indexOf("=");
      if (eq < 0) return;
      const key = line.slice(0, eq);
      if (key !== "out_time_us" && key !== "out_time_ms") return;
      const micros = Number(line.slice(eq + 1));
      if (!Number.isFinite(micros) || durationMs <= 0) return;
      onProgress(Math.min(1, Math.max(0, micros / 1000 / durationMs)));
    };

    child.stdout.on("data", (chunk: Buffer) => {
      stdoutBuffer += chunk.toString();
      const lines = stdoutBuffer.split("\n");
      stdoutBuffer = lines.pop() ?? "";
      lines.forEach(handleLine);
    });
    child.stderr.on("data", (chunk: Buffer) => {
      stderrBuffer += chunk.toString();
      if (stderrBuffer.length > 65536) stderrBuffer = stderrBuffer.slice(-65536);
    });

    let settled = false;
    const finish = async (error: Error | null) => {
      if (settled) return;
      settled = true;
      signal?.removeEventListener("abort", onAbort);
      clearTimeout(killTimer);
      if (error) {
        await rm(part, { force: true });
        reject(error);
        return;
      }
      try {
        await rename(part, dst);
        onProgress(1);
        resolve();
      } catch (e) {
        await rm(part, { force: true });
        reject(e instanceof Error ? e : new Error(String(e)));
      }
    };

    child.on("error", (e) => {
      void finish(new Error(`ffmpeg failed to start: ${e.message}`));
    });
    child.on("close", (code) => {
      if (aborted) {
        void finish(abortError());
      } else if (code === 0) {
        void finish(null);
      } else {
        const tail = stderrBuffer.trim().split("\n").slice(-STDERR_TAIL_LINES).join("\n");
        void finish(new Error(`ffmpeg exited with code ${code}:\n${tail}`));
      }
    });
  });
}
