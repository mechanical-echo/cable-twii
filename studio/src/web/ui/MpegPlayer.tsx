import { useEffect, useRef, useState } from "react";
import { playUrl } from "../api";
import { formatDuration } from "../format";

interface Destination {
  write(data: ArrayBuffer): void;
}

interface JSMpegPlayer {
  currentTime: number;
  play(): void;
  pause(): void;
  destroy(): void;
}

interface JSMpegGlobal {
  Player: new (url: string, options: Record<string, unknown>) => JSMpegPlayer;
}

declare global {
  interface Window {
    JSMpeg?: JSMpegGlobal;
  }
}

const SCRIPT = "/vendor/jsmpeg.min.js";
const NO_PICTURE_MS = 8000;
const HEADROOM_S = 4;
const TICK_MS = 250;

let loading: Promise<JSMpegGlobal> | null = null;

function loadJSMpeg(): Promise<JSMpegGlobal> {
  if (window.JSMpeg) return Promise.resolve(window.JSMpeg);
  loading ??= new Promise((resolve, reject) => {
    const script = document.createElement("script");
    script.src = SCRIPT;
    script.onload = () => (window.JSMpeg ? resolve(window.JSMpeg) : reject(new Error("player missing")));
    script.onerror = () => {
      loading = null;
      reject(new Error("player script failed"));
    };
    document.head.appendChild(script);
  });
  return loading;
}

class FetchSource {
  streaming = false;
  established = false;
  completed = false;
  progress = 0;
  private destination: Destination | null = null;
  private reader: ReadableStreamDefaultReader<Uint8Array> | null = null;
  private reading = false;
  private readonly aborter = new AbortController();
  private readonly onEstablished?: (source: FetchSource) => void;
  private readonly onCompleted?: (source: FetchSource) => void;
  private readonly onFailed?: () => void;

  constructor(
    private readonly url: string,
    options: { onSourceEstablished?: (s: FetchSource) => void; onSourceCompleted?: (s: FetchSource) => void; onFetchFailed?: () => void },
  ) {
    this.onEstablished = options.onSourceEstablished;
    this.onCompleted = options.onSourceCompleted;
    this.onFailed = options.onFetchFailed;
  }

  connect(destination: Destination): void {
    this.destination = destination;
  }

  start(): void {
    fetch(this.url, { signal: this.aborter.signal })
      .then((res) => {
        if (!res.ok || res.body === null) throw new Error(`HTTP ${res.status}`);
        this.reader = res.body.getReader();
        return this.next();
      })
      .catch(() => {
        if (!this.aborter.signal.aborted) this.onFailed?.();
      });
  }

  resume(headroom: number): void {
    if (!this.reading && !this.completed && this.reader !== null && headroom < HEADROOM_S) void this.next();
  }

  destroy(): void {
    this.aborter.abort();
  }

  private async next(): Promise<void> {
    if (this.reader === null || this.reading) return;
    this.reading = true;
    try {
      const { done, value } = await this.reader.read();
      if (done) {
        this.completed = true;
        this.progress = 1;
        this.onCompleted?.(this);
        return;
      }
      const first = !this.established;
      this.established = true;
      if (first) this.onEstablished?.(this);
      this.destination?.write(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength) as ArrayBuffer);
    } catch {
      if (!this.aborter.signal.aborted) this.onFailed?.();
    } finally {
      this.reading = false;
    }
  }
}

interface Props {
  path: string;
  durationMs: number;
  aspect: "16:9" | "4:3";
}

export function MpegPlayer({ path, durationMs, aspect }: Props) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const player = useRef<JSMpegPlayer | null>(null);
  const timers = useRef<number[]>([]);
  const [started, setStarted] = useState(false);
  const [playing, setPlaying] = useState(false);
  const [time, setTime] = useState(0);
  const [failed, setFailed] = useState(false);
  const total = durationMs / 1000;

  function stop() {
    for (const t of timers.current) window.clearInterval(t);
    timers.current = [];
    player.current?.destroy();
    player.current = null;
  }

  useEffect(() => stop, []);

  async function open(at: number) {
    stop();
    setFailed(false);
    setStarted(true);
    setTime(at);
    let JSMpeg: JSMpegGlobal;
    try {
      JSMpeg = await loadJSMpeg();
    } catch {
      setFailed(true);
      return;
    }
    if (canvas.current === null) return;
    let picture = false;
    const created = new JSMpeg.Player(playUrl(path, at), {
      canvas: canvas.current,
      source: FetchSource,
      autoplay: true,
      audio: true,
      onVideoDecode: () => {
        picture = true;
      },
      onPlay: () => setPlaying(true),
      onPause: () => setPlaying(false),
      onEnded: () => setPlaying(false),
      onFetchFailed: () => setFailed(true),
    });
    player.current = created;
    setPlaying(true);
    const watch = window.setTimeout(() => {
      if (!picture && player.current === created) setFailed(true);
    }, NO_PICTURE_MS);
    const tick = window.setInterval(() => {
      if (player.current === created) setTime(at + Math.max(0, created.currentTime || 0));
    }, TICK_MS);
    timers.current.push(watch, tick);
  }

  function toggle() {
    if (!started || player.current === null) {
      void open(Math.floor(time));
      return;
    }
    if (playing) player.current.pause();
    else player.current.play();
  }

  function seek(event: React.MouseEvent<HTMLDivElement>) {
    const box = event.currentTarget.getBoundingClientRect();
    const ratio = Math.min(1, Math.max(0, (event.clientX - box.left) / box.width));
    void open(Math.floor(ratio * total));
  }

  return (
    <div className="ui-player">
      <div className={aspect === "16:9" ? "ui-player-screen wide" : "ui-player-screen"}>
        <canvas ref={canvas} />
        {!started && (
          <button className="ui-player-start" aria-label="Play" onClick={() => void open(0)}>
            ▶
          </button>
        )}
        {failed && <div className="ui-player-failed">Can't play this file here</div>}
      </div>
      <div className="ui-player-bar">
        <button className="ui-pill small" onClick={toggle} aria-label={playing ? "Pause" : "Play"}>
          {playing ? "❚❚" : "▶"}
        </button>
        <div className="ui-player-track" role="slider" aria-label="Position" aria-valuemin={0} aria-valuemax={Math.round(total)} aria-valuenow={Math.round(time)} onClick={seek}>
          <i style={{ width: `${total > 0 ? Math.min(100, (time / total) * 100) : 0}%` }} />
        </div>
        <span className="ui-player-time">
          {formatDuration(time * 1000)} / {formatDuration(durationMs)}
        </span>
      </div>
    </div>
  );
}
