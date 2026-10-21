import { useState } from "react";
import { putShowMeta } from "../api";
import { formatDuration } from "../format";
import type { ShowNode, WebLibrary } from "../tree";
import { descriptionKey, seasonLabel } from "../tree";
import { Dialog } from "./Dialog";

interface Props {
  node: ShowNode;
  onClose: () => void;
  onSaved: (library: WebLibrary) => void;
}

export function ShowMetaDialog({ node, onClose, onSaved }: Props) {
  const { show } = node;
  const [name, setName] = useState(show.name);
  const [descriptions, setDescriptions] = useState<Record<string, string>>(() => {
    const out: Record<string, string> = {};
    for (const ep of show.episode) out[descriptionKey(ep)] = ep.description;
    return out;
  });
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function save() {
    setBusy(true);
    setError(null);
    const episodes: Record<string, string> = {};
    for (const [key, text] of Object.entries(descriptions)) if (text.trim() !== "") episodes[key] = text.trim();
    try {
      onSaved(await putShowMeta({ channelFolder: show.channel, showFolder: show.folder, name: name.trim(), episodes }));
      onClose();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  return (
    <Dialog title={`Edit info · ${show.folder}`} onClose={onClose}>
      <div className="form-grid">
        <label>
          Show name
          <input value={name} onChange={(e) => setName(e.target.value)} />
        </label>
      </div>
      {node.seasons.map((s) => (
        <div key={s.season}>
          <h4>{seasonLabel(s.season)}</h4>
          {s.episodes.map((ep) => {
            const key = descriptionKey(ep);
            return (
              <label key={ep.path} className="desc">
                <span>
                  {ep.title} <span className="muted">{formatDuration(ep.duration_ms)}</span>
                </span>
                <input value={descriptions[key] ?? ""} onChange={(e) => setDescriptions((prev) => ({ ...prev, [key]: e.target.value }))} />
              </label>
            );
          })}
        </div>
      ))}
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button onClick={onClose}>Cancel</button>
        <button className="primary" disabled={busy} onClick={save}>
          Save
        </button>
      </div>
    </Dialog>
  );
}
