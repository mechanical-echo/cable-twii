import { useEffect, useState } from "react";
import { getShowMeta, putShowMeta, uploadIcon } from "../api";
import { formatDuration } from "../format";
import { formatEpisodeCode } from "../preview";
import { type ShowNode, type WebLibrary, descriptionKey, seasonLabel } from "../tree";
import { PillButton, TextField } from "../ui/controls";
import { Banner, EmptyState } from "../ui/layout";

interface Props {
  node: ShowNode;
  channelFolder: string;
  onClose: () => void;
  onSaved: (library: WebLibrary, iconChanged: boolean) => void;
}

export function ShowInfoDialog({ node, channelFolder, onClose, onSaved }: Props) {
  const { show } = node;
  const [name, setName] = useState("");
  const [descriptions, setDescriptions] = useState<Record<string, string>>({});
  const [icon, setIcon] = useState<File | null>(null);
  const [mtimeMs, setMtimeMs] = useState<number | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let current = true;
    getShowMeta(channelFolder, show.folder).then(
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
  }, [channelFolder, show.folder]);

  async function save() {
    if (mtimeMs === null) return;
    setBusy(true);
    setError(null);
    const episodes: Record<string, string> = {};
    for (const [key, text] of Object.entries(descriptions)) if (text.trim() !== "") episodes[key] = text.trim();
    try {
      const library = await putShowMeta({ channelFolder, showFolder: show.folder, name: name.trim(), episodes, mtimeMs });
      if (icon !== null) await uploadIcon("show", channelFolder, show.folder, icon);
      onSaved(library, icon !== null);
      onClose();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  return (
    <div className="overlay">
      <div className="ui-dialog wide" role="dialog" aria-modal="true" aria-label={`Edit ${show.name}`}>
        <h2>Edit {show.name}</h2>
        {mtimeMs === null && error === null && <EmptyState>Loading…</EmptyState>}
        {mtimeMs !== null && (
          <div className="ui-stack">
            <TextField label="Show name" value={name} onChange={setName} width="full" />
            <label className="ui-field">
              <span className="ui-label">Icon</span>
              <input className="ui-file-input" type="file" accept="image/*" onChange={(e) => setIcon(e.target.files?.[0] ?? null)} />
            </label>
            {node.seasons.map((s) => (
              <div key={s.season} className="ui-stack">
                <h3 className="ui-subhead">{seasonLabel(s.season)}</h3>
                {s.episodes.map((ep) => {
                  const key = descriptionKey(ep);
                  const code = ep.season >= 1 ? formatEpisodeCode(ep.season, ep.episode) : "";
                  return (
                    <TextField
                      key={ep.path}
                      label={`${code !== "" ? `${code} · ` : ""}${ep.title} (${formatDuration(ep.duration_ms)})`}
                      value={descriptions[key] ?? ""}
                      placeholder="Description shown on the Wii"
                      onChange={(text) => setDescriptions((prev) => ({ ...prev, [key]: text }))}
                      width="full"
                    />
                  );
                })}
              </div>
            ))}
          </div>
        )}
        {error !== null && <Banner kind="error">{error}</Banner>}
        <div className="ui-dialog-actions">
          <PillButton onClick={onClose}>Cancel</PillButton>
          <PillButton kind="primary" disabled={busy || mtimeMs === null} onClick={() => void save()}>
            Save
          </PillButton>
        </div>
      </div>
    </div>
  );
}
