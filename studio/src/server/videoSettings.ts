export type Aspect = "16:9" | "4:3";

export interface VideoProfile {
  videoBitrate: string;
  videoMaxrate: string;
  videoBufsize: string;
  audioBitrate: string;
}

export const DRIVE_PROFILE: VideoProfile = { videoBitrate: "1500k", videoMaxrate: "2000k", videoBufsize: "1000k", audioBitrate: "192k" };
// The Wii's Wi-Fi cannot sustain the drive bitrate (docs/decisions/0004-server-streaming.md).
export const SERVER_PROFILE: VideoProfile = { videoBitrate: "900k", videoMaxrate: "1100k", videoBufsize: "600k", audioBitrate: "128k" };

export const MIN_DURATION_MS = 1000;
export const WIDE_ASPECT_THRESHOLD = 1.5;
export const FRAME_SIZE: Record<Aspect, { width: number; height: number }> = {
  "16:9": { width: 640, height: 360 },
  "4:3": { width: 480, height: 360 },
};
export const FRAME_RATE = 30;
export const VIDEO_CODEC = "mpeg1video";
export const GOP_SIZE = 15;
export const LOUDNORM = "loudnorm=I=-16:TP=-1.5:LRA=11";
export const AUDIO_CODEC = "mp2";
export const AUDIO_SAMPLE_RATE = 48000;
export const AUDIO_CHANNELS = 2;
export const CONTAINER_FORMAT = "mpeg";
export const KILL_GRACE_MS = 2000;
export const STDERR_TAIL_LINES = 20;
