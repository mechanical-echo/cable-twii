export type Aspect = "16:9" | "4:3";

export const MIN_DURATION_MS = 1000;
export const WIDE_ASPECT_THRESHOLD = 1.5;
export const FRAME_SIZE: Record<Aspect, { width: number; height: number }> = {
  "16:9": { width: 640, height: 360 },
  "4:3": { width: 480, height: 360 },
};
export const FRAME_RATE = 30;
export const VIDEO_CODEC = "mpeg1video";
export const VIDEO_BITRATE = "1500k";
export const VIDEO_MAXRATE = "2000k";
export const VIDEO_BUFSIZE = "1000k";
export const GOP_SIZE = 15;
export const LOUDNORM = "loudnorm=I=-16:TP=-1.5:LRA=11";
export const AUDIO_CODEC = "mp2";
export const AUDIO_SAMPLE_RATE = 48000;
export const AUDIO_CHANNELS = 2;
export const AUDIO_BITRATE = "192k";
export const CONTAINER_FORMAT = "mpeg";
export const KILL_GRACE_MS = 2000;
export const STDERR_TAIL_LINES = 20;
