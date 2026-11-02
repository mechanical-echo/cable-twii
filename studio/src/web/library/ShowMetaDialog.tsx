import { useEffect, useState } from "react";
import { getShowMeta, putShowMeta } from "../api";
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
  const [name, setName] = useState("");
  const [descriptions, setDescriptions] = useState<Record<string, string>>({});
  const [mtimeMs, setMtimeMs] = useState<number | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let current = true;
    getShowMeta(show.channel, show.folder).then(
      (loaded) => {
        if (!current) return;
        setName(loaded.meta.name ?? show.folder);
        setDescriptions(loaded.meta.episodes);
        setMtimeMs(loaded.mtimeMs);
      },
      (err: unknown) => {
        if (current) setError((err as Error).message);
      },
    );
    return () => {
      current = false;
    };
  }, [show.channel, show.folder]);

  async function save() {
    if (mtimeMs === null) return;
    setBusy(true);
    setError(null);
    const episodes: Record<string, string> = {};
    for (const [key, text] of Object.entries(descriptions)) if (text.trim() !== "") episodes[key] = text.trim();
    try {
      onSaved(await putShowMeta({ channelFolder: show.channel, showFolder: show.folder, name: name.trim(), episodes, mtimeMs }));
      onClose();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  return (
    <Dialog title={`Edit info · ${show.folder}`} onClose={onClose}>
      {mtimeMs === null && error === null && <p className="muted">Loading…</p>}
      {mtimeMs !== null && (
        <>
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
        </>
      )}
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button onClick={onClose}>Cancel</button>
        <button className="primary" disabled={busy || mtimeMs === null} onClick={save}>
          Save
        </button>
      </div>
    </Dialog>
  );
}
