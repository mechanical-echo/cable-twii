import { useCallback, useEffect, useMemo, useState } from "react";
import { type Config, type Issue, getConfig, getLibrary, mediaUrl, putConfig, uploadIcon } from "../api";
import {
  type ChannelDraft,
  applyDelete,
  applyEdit,
  draftFolder,
  draftFromChannel,
  folderSuggestions,
  iconPath,
  newDraft,
  setMode,
  sortChannels,
  validateDraft
} from "../channels";
import { FailureBanner } from "../FailureBanner";
import { type Failure, toFailure } from "../failure";
import { Dialog } from "../library/Dialog";

interface Editing {
  original: number | null;
  draft: ChannelDraft;
}

function IconImage({ src }: { src: string }) {
  const [failed, setFailed] = useState<string | null>(null);
  return <img className="icon" src={src} alt="" style={{ visibility: failed === src ? "hidden" : "visible" }} onError={() => setFailed(src)} />;
}

export function ChannelsPage() {
  const [config, setConfig] = useState<Config | null>(null);
  const [mtimeMs, setMtimeMs] = useState(0);
  const [folders, setFolders] = useState<string[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [editing, setEditing] = useState<Editing | null>(null);
  const [formErrors, setFormErrors] = useState<string[]>([]);
  const [failure, setFailure] = useState<Failure | null>(null);
  const [warnings, setWarnings] = useState<Issue[]>([]);
  const [busy, setBusy] = useState(false);
  const [iconStamp, setIconStamp] = useState(0);
  const [iconError, setIconError] = useState<string | null>(null);

  const load = useCallback(() => {
    setError(null);
    setFailure(null);
    setEditing(null);
    getConfig()
      .then((result) => {
        setConfig(result.config);
        setMtimeMs(result.mtimeMs);
      })
      .catch((err: Error) => setError(err.message));
    getLibrary()
      .then((library) => setFolders(folderSuggestions(library)))
      .catch(() => setFolders([]));
  }, []);

  useEffect(load, [load]);

  const channels = useMemo(() => (config ? sortChannels(config.channel) : []), [config]);

  function openForm(next: Editing | null) {
    setEditing(next);
    setFormErrors([]);
    setFailure(null);
    setIconError(null);
  }

  async function persist(next: Config, onSaved: () => void) {
    setBusy(true);
    setFailure(null);
    try {
      const result = await putConfig(next, mtimeMs);
      setConfig(next);
      setMtimeMs(result.mtimeMs);
      setWarnings(result.issues.filter((i) => i.level === "warning"));
      onSaved();
    } catch (err) {
      setFailure(toFailure(err));
    } finally {
      setBusy(false);
    }
  }

  async function save() {
    if (!config || !editing) return;
    const errors = validateDraft(editing.draft, config.channel, editing.original);
    setFormErrors(errors);
    if (errors.length > 0) return;
    await persist(applyEdit(config, editing.original, editing.draft), () => setEditing(null));
  }

  async function remove(number: number, name: string) {
    if (!config) return;
    if (!window.confirm(`Delete channel ${number} ${name}? Its files stay on the drive.`)) return;
    await persist(applyDelete(config, number), () => undefined);
  }

  async function chooseIcon(file: File | undefined) {
    if (!file || !editing) return;
    const folder = draftFolder(editing.draft);
    if (folder === "") {
      setIconError("Enter a name or folder first.");
      return;
    }
    setIconError(null);
    setBusy(true);
    try {
      await uploadIcon("channel", folder, null, file);
      setIconStamp(Date.now());
      setEditing((current) => {
        if (!current) return current;
        const { icon: _icon, ...draft } = current.draft;
        return { ...current, draft };
      });
    } catch (err) {
      setIconError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  const iconUrl = (path: string) => `${mediaUrl(path)}?v=${iconStamp}`;

  function patch(change: Partial<ChannelDraft>) {
    setEditing((current) => (current ? { ...current, draft: { ...current.draft, ...change } } : current));
  }

  const banner = <FailureBanner failure={failure} onReload={load} />;

  const draft = editing?.draft;

  return (
    <section>
      <div className="row">
        <h2 className="grow">Channels</h2>
        <button className="primary" disabled={!config || busy} onClick={() => config && openForm({ original: null, draft: newDraft(config.channel) })}>
          Add channel
        </button>
      </div>
      {error !== null && (
        <div className="banner error row">
          <span className="grow">{error}</span>
          <button onClick={load}>Retry</button>
        </div>
      )}
      {editing === null && banner}
      {warnings.length > 0 && (
        <div className="banner warn">
          <ul>
            {warnings.map((issue, i) => (
              <li key={i}>{issue.message}</li>
            ))}
          </ul>
        </div>
      )}
      {config !== null && channels.length === 0 && <p className="muted">No channels yet.</p>}
      {channels.length > 0 && (
        <div className="card table-wrap">
          <table>
            <thead>
              <tr>
                <th>#</th>
                <th></th>
                <th>Name</th>
                <th>Category</th>
                <th>Ads</th>
                <th>Mode</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {channels.map((channel) => (
                <tr key={channel.number}>
                  <td>{channel.number}</td>
                  <td>
                    <IconImage src={iconUrl(iconPath(channel))} />
                  </td>
                  <td>{channel.name}</td>
                  <td>{channel.category}</td>
                  <td>{channel.ads ? "On" : "Off"}</td>
                  <td>{channel.mode}</td>
                  <td>
                    <div className="row end">
                      <button disabled={busy} onClick={() => openForm({ original: channel.number, draft: draftFromChannel(channel) })}>
                        Edit
                      </button>
                      <button disabled={busy} onClick={() => remove(channel.number, channel.name)}>
                        Delete
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
      {editing !== null && draft !== undefined && (
        <Dialog title={editing.original === null ? "Add channel" : `Edit channel ${editing.original}`} onClose={() => openForm(null)}>
          <div className="form-grid">
            <label>
              Number (1–999)
              <input className="num" value={draft.number} onChange={(e) => patch({ number: e.target.value })} />
            </label>
            <label>
              Name
              <input value={draft.name} onChange={(e) => patch({ name: e.target.value })} />
            </label>
            <label>
              Category
              <input value={draft.category} onChange={(e) => patch({ category: e.target.value })} />
            </label>
            <label>
              Folder
              <input
                list="channel-folders"
                placeholder={draft.name.trim()}
                value={draft.folder}
                onChange={(e) => patch({ folder: e.target.value })}
              />
              <datalist id="channel-folders">
                {folders.map((folder) => (
                  <option key={folder} value={folder} />
                ))}
              </datalist>
            </label>
            <label>
              Mode
              <select value={draft.mode} onChange={(e) => setEditing({ ...editing, draft: setMode(draft, e.target.value as ChannelDraft["mode"]) })}>
                <option value="timed">timed</option>
                <option value="looping">looping</option>
              </select>
            </label>
            <label>
              Ads
              <input type="checkbox" checked={draft.ads} onChange={(e) => patch({ ads: e.target.checked })} />
            </label>
          </div>
          <div className="row">
            {draftFolder(draft) !== "" && (
              <IconImage src={iconUrl(iconPath(draft))} />
            )}
            <label className="desc">
              Icon (saved right away as {draftFolder(draft) || "<folder>"}/channel.png)
              <input type="file" accept="image/*" disabled={busy} onChange={(e) => void chooseIcon(e.target.files?.[0])} />
            </label>
          </div>
          {iconError !== null && <div className="banner error">{iconError}</div>}
          <p className="muted">The schedule is built on the Schedule page.</p>
          {formErrors.length > 0 && (
            <div className="banner error">
              <ul>
                {formErrors.map((message) => (
                  <li key={message}>{message}</li>
                ))}
              </ul>
            </div>
          )}
          {banner}
          <div className="row end">
            <button onClick={() => openForm(null)}>Cancel</button>
            <button className="primary" disabled={busy} onClick={save}>
              Save
            </button>
          </div>
        </Dialog>
      )}
    </section>
  );
}
