import { useState } from "react";
import { type ScanResult, importAds } from "../api";
import { baseName, formatDuration } from "../format";
import { Dialog } from "./Dialog";

interface Props {
  dir: string;
  files: ScanResult[];
  channels: string[];
  onClose: () => void;
  onImported: () => void;
}

export function ImportAdsDialog({ dir, files, channels, onClose, onImported }: Props) {
  const usable = (f: ScanResult) => !f.tooShort && f.error === undefined;
  const [channel, setChannel] = useState("");
  const [picked, setPicked] = useState(() => new Set(files.filter(usable).map((f) => f.source)));
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const toggle = (source: string, on: boolean) =>
    setPicked((prev) => {
      const next = new Set(prev);
      if (on) next.add(source);
      else next.delete(source);
      return next;
    });

  async function submit() {
    setBusy(true);
    setError(null);
    try {
      const chosen = files.filter((f) => picked.has(f.source));
      const result = await importAds({ channelFolder: channel, files: chosen.map((f) => ({ source: f.source, aspect: f.aspect })) });
      onImported();
      if (result.rejected.length > 0) {
        setError(`Imported ${result.jobs.length}. Skipped: ${result.rejected.map((x) => `${baseName(x.source)} (${x.reason})`).join("; ")}`);
        setBusy(false);
        return;
      }
      onClose();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  return (
    <Dialog title="Add ads" onClose={onClose}>
      <div className="form-grid">
        <label>
          Play on
          <select value={channel} onChange={(e) => setChannel(e.target.value)}>
            <option value="">All channels</option>
            {channels.map((c) => (
              <option key={c} value={c}>
                {c}
              </option>
            ))}
          </select>
        </label>
      </div>
      <p className="muted">{dir}</p>
      <ul className="plain">
        {files.map((f) => (
          <li key={f.source} className={usable(f) ? undefined : "blocked"}>
            <label>
              <input type="checkbox" checked={picked.has(f.source)} disabled={!usable(f)} onChange={(e) => toggle(f.source, e.target.checked)} />{" "}
              {baseName(f.source)} <span className="muted">{formatDuration(f.durationMs)}</span>
              {f.error !== undefined && <span className="muted"> · {f.error}</span>}
            </label>
          </li>
        ))}
      </ul>
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button onClick={onClose}>Cancel</button>
        <button className="primary" disabled={busy || picked.size === 0} onClick={submit}>
          Import
        </button>
      </div>
    </Dialog>
  );
}
