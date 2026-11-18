import { useState } from "react";
import { type ScanResult, discardUploaded, importAds } from "../api";
import { NEW_CHANNEL } from "../channels";
import { baseName, formatDuration } from "../format";
import { describeRejected, isUsable } from "../importing";
import { ChannelPicker } from "./ChannelPicker";
import { Dialog } from "./Dialog";

interface Props {
  dir: string;
  files: ScanResult[];
  uploaded: boolean;
  channels: string[];
  onClose: () => void;
  onImported: () => void;
}

export function ImportAdsDialog({ dir, files, uploaded, channels, onClose, onImported }: Props) {
  const [channel, setChannel] = useState("");
  const [newChannel, setNewChannel] = useState("");
  const [picked, setPicked] = useState(() => new Set(files.filter(isUsable).map((f) => f.source)));
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [imported, setImported] = useState(false);

  const toggle = (source: string, on: boolean) =>
    setPicked((prev) => {
      const next = new Set(prev);
      if (on) next.add(source);
      else next.delete(source);
      return next;
    });

  const channelFolder = (channel === NEW_CHANNEL ? newChannel : channel).trim();

  async function submit() {
    setBusy(true);
    setError(null);
    try {
      const chosen = files.filter((f) => picked.has(f.source));
      const result = await importAds({ channelFolder, files: chosen.map((f) => ({ source: f.source, aspect: f.aspect })) });
      setImported(true);
      onImported();
      let skipped = describeRejected(result.jobs.length, result.rejected);
      if (uploaded) {
        try {
          const unticked = files.filter((f) => !picked.has(f.source)).map((f) => f.source);
          await discardUploaded([...unticked, ...result.rejected.map((r) => r.source)]);
        } catch (err) {
          skipped = `${skipped ?? "Imported."} The unticked uploads could not be deleted: ${(err as Error).message}`;
        }
      }
      if (skipped === null) {
        onClose();
        return;
      }
      setError(skipped);
      setBusy(false);
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  async function close() {
    if (uploaded && !imported && window.confirm("Delete the uploaded files?")) {
      setBusy(true);
      try {
        await discardUploaded(files.map((f) => f.source));
      } catch (err) {
        setError(`Some uploads could not be deleted: ${(err as Error).message}`);
        setBusy(false);
        return;
      }
    }
    onClose();
  }

  return (
    <Dialog title="Add ads" busy={busy} onClose={close}>
      <div className="form-grid">
        <label>
          Play on
          <ChannelPicker all choices={channels} value={channel} newName={newChannel} onChange={setChannel} onNewName={setNewChannel} />
        </label>
      </div>
      <p className="muted">{dir}</p>
      <ul className="plain">
        {files.map((f) => (
          <li key={f.source} className={isUsable(f) ? undefined : "blocked"}>
            <label>
              <input type="checkbox" checked={picked.has(f.source)} disabled={!isUsable(f)} onChange={(e) => toggle(f.source, e.target.checked)} />{" "}
              {baseName(f.source)} <span className="muted">{formatDuration(f.durationMs)}</span>
              {f.error !== undefined && <span className="muted"> · {f.error}</span>}
            </label>
          </li>
        ))}
      </ul>
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button disabled={busy} onClick={close}>{imported ? "Close" : "Cancel"}</button>
        <button className="primary" disabled={busy || imported || picked.size === 0 || (channel === NEW_CHANNEL && channelFolder === "")} onClick={submit}>
          Import
        </button>
      </div>
    </Dialog>
  );
}
