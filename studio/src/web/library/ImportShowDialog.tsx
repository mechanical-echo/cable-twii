import { useState } from "react";
import { type ImportEpisode, type ScanResult, importShow, uploadIcon } from "../api";
import { baseName } from "../format";
import { describeRejected, isUsable } from "../importing";
import { Dialog } from "./Dialog";

interface Props {
  dir: string;
  files: ScanResult[];
  channels: string[];
  onClose: () => void;
  onImported: () => void;
}

interface Row {
  scan: ScanResult;
  include: boolean;
  season: string;
  episode: string;
  title: string;
  description: string;
}

function toRow(scan: ScanResult): Row {
  return {
    scan,
    include: isUsable(scan),
    season: scan.guess.season === null ? "" : String(scan.guess.season),
    episode: scan.guess.episode === null ? "" : String(scan.guess.episode),
    title: scan.guess.title,
    description: "",
  };
}

function whole(text: string): number | null {
  const t = text.trim();
  return /^\d+$/.test(t) ? Number(t) : null;
}

export function ImportShowDialog({ dir, files, channels, onClose, onImported }: Props) {
  const [channel, setChannel] = useState(channels[0] ?? "");
  const [showName, setShowName] = useState(baseName(dir));
  const [rows, setRows] = useState(() => files.map(toRow));
  const [icon, setIcon] = useState<File | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [imported, setImported] = useState(false);

  const patch = (i: number, change: Partial<Row>) => setRows((prev) => prev.map((r, k) => (k === i ? { ...r, ...change } : r)));
  const chosen = rows.filter((r) => r.include);
  const canImport = !busy && !imported && channel.trim() !== "" && showName.trim() !== "" && chosen.length > 0;

  async function submit() {
    setBusy(true);
    setError(null);
    const folder = showName.trim();
    const channelFolder = channel.trim();
    const episodes: ImportEpisode[] = chosen.map((r) => ({
      source: r.scan.source,
      season: whole(r.season),
      episode: whole(r.episode),
      title: r.title.trim(),
      description: r.description.trim(),
      aspect: r.scan.aspect,
    }));
    let result;
    try {
      result = await importShow({ channelFolder, showFolder: folder, showName: folder, episodes });
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
      return;
    }
    setImported(true);
    setBusy(false);
    onImported();
    const notes: string[] = [];
    const skipped = describeRejected(result.jobs.length, result.rejected);
    if (skipped !== null) notes.push(skipped);
    if (icon !== null && result.jobs.length > 0) {
      try {
        await uploadIcon("show", channelFolder, folder, icon);
      } catch (err) {
        notes.push(`Imported, but the icon failed: ${(err as Error).message}`);
      }
    }
    if (notes.length === 0) onClose();
    else setError(notes.join(" "));
  }

  return (
    <Dialog title="Add show" onClose={onClose}>
      <div className="form-grid">
        <label>
          Channel
          <input list="channel-options" value={channel} onChange={(e) => setChannel(e.target.value)} />
          <datalist id="channel-options">
            {channels.map((c) => (
              <option key={c} value={c} />
            ))}
          </datalist>
        </label>
        <label>
          Show name
          <input value={showName} onChange={(e) => setShowName(e.target.value)} />
        </label>
        <label>
          Show icon
          <input type="file" accept="image/*" onChange={(e) => setIcon(e.target.files?.[0] ?? null)} />
        </label>
      </div>
      <p className="muted">{dir}</p>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th></th>
              <th>File</th>
              <th>Season</th>
              <th>Episode</th>
              <th>Title</th>
              <th>Description</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((r, i) => {
              const blocked = !isUsable(r.scan);
              return (
                <tr key={r.scan.source} className={blocked ? "blocked" : undefined}>
                  <td>
                    <input type="checkbox" checked={r.include} disabled={blocked} onChange={(e) => patch(i, { include: e.target.checked })} />
                  </td>
                  <td title={r.scan.error ?? (r.scan.tooShort ? "Too short" : r.scan.source)}>{baseName(r.scan.source)}</td>
                  <td>
                    <input className="num" value={r.season} onChange={(e) => patch(i, { season: e.target.value })} />
                  </td>
                  <td>
                    <input className="num" value={r.episode} onChange={(e) => patch(i, { episode: e.target.value })} />
                  </td>
                  <td>
                    <input value={r.title} onChange={(e) => patch(i, { title: e.target.value })} />
                  </td>
                  <td>
                    <input value={r.description} onChange={(e) => patch(i, { description: e.target.value })} />
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button onClick={onClose}>{imported ? "Close" : "Cancel"}</button>
        <button className="primary" disabled={!canImport} onClick={submit}>
          Import
        </button>
      </div>
    </Dialog>
  );
}
