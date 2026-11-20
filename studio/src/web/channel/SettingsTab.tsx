import { useCallback, useEffect, useState } from "react";
import { type Config, type Issue, getConfig, getLibrary, mediaUrl, putConfig, uploadIcon } from "../api";
import { type ChannelDraft, applyDelete, applyEdit, draftFolder, draftFromChannel, folderSuggestions, iconPath, setMode, validateDraft } from "../channels";
import { FailureBanner } from "../FailureBanner";
import { type Failure, toFailure } from "../failure";
import { IconImage } from "../IconImage";
import { ConfirmDialog } from "../ui/ConfirmDialog";
import { Checkbox, PillButton, Select, TextField } from "../ui/controls";
import { Banner, EmptyState, Panel } from "../ui/layout";

interface Props {
  number: number;
  onDeleted: () => void;
  onSaved: (number: number) => void;
}

export function SettingsTab({ number, onDeleted, onSaved }: Props) {
  const [config, setConfig] = useState<Config | null>(null);
  const [mtimeMs, setMtimeMs] = useState(0);
  const [draft, setDraft] = useState<ChannelDraft | null>(null);
  const [folders, setFolders] = useState<string[]>([]);
  const [errors, setErrors] = useState<string[]>([]);
  const [failure, setFailure] = useState<Failure | null>(null);
  const [warnings, setWarnings] = useState<Issue[]>([]);
  const [busy, setBusy] = useState(false);
  const [deleting, setDeleting] = useState(false);
  const [iconStamp, setIconStamp] = useState(0);
  const [iconError, setIconError] = useState<string | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);

  const load = useCallback(() => {
    setFailure(null);
    setLoadError(null);
    getConfig()
      .then((result) => {
        setConfig(result.config);
        setMtimeMs(result.mtimeMs);
        const channel = result.config.channel.find((c) => c.number === number);
        setDraft(channel === undefined ? null : draftFromChannel(channel));
      })
      .catch((err: Error) => setLoadError(err.message));
    getLibrary()
      .then((library) => setFolders(folderSuggestions(library)))
      .catch(() => setFolders([]));
  }, [number]);

  useEffect(load, [load]);

  async function persist(next: Config, done: () => void) {
    setBusy(true);
    setFailure(null);
    try {
      const result = await putConfig(next, mtimeMs);
      setConfig(next);
      setMtimeMs(result.mtimeMs);
      setWarnings(result.issues.filter((i) => i.level === "warning"));
      done();
    } catch (err) {
      setFailure(toFailure(err));
    } finally {
      setBusy(false);
    }
  }

  async function save() {
    if (config === null || draft === null) return;
    const problems = validateDraft(draft, config.channel, number);
    setErrors(problems);
    if (problems.length > 0) return;
    await persist(applyEdit(config, number, draft), () => onSaved(Number(draft.number)));
  }

  async function chooseIcon(file: File | undefined) {
    if (file === undefined || draft === null) return;
    const folder = draftFolder(draft);
    if (folder === "") {
      setIconError("Enter a name or folder first.");
      return;
    }
    setIconError(null);
    setBusy(true);
    try {
      await uploadIcon("channel", folder, null, file);
      setIconStamp(Date.now());
      const { icon: _icon, ...rest } = draft;
      setDraft(rest);
    } catch (err) {
      setIconError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  if (loadError !== null) return <Banner kind="error">{loadError}</Banner>;
  if (config === null) return <EmptyState>Loading…</EmptyState>;
  if (draft === null) return <EmptyState>This channel no longer exists.</EmptyState>;
  const original = config.channel.find((c) => c.number === number);
  const patch = (change: Partial<ChannelDraft>) => setDraft({ ...draft, ...change });

  return (
    <>
      <Panel>
        <div className="ui-settings">
          <TextField label="Name" value={draft.name} onChange={(name) => patch({ name })} width="full" />
          <TextField label="Number (1–999)" value={draft.number} onChange={(value) => patch({ number: value })} width="short" />
          <TextField label="Category" value={draft.category} onChange={(category) => patch({ category })} width="full" />
          <label className="ui-field full">
            <span className="ui-label">Folder on the drive</span>
            <input className="ui-input full" list="channel-folders" placeholder={draft.name.trim()} value={draft.folder} onChange={(e) => patch({ folder: e.target.value })} />
            <datalist id="channel-folders">
              {folders.map((folder) => (
                <option key={folder} value={folder} />
              ))}
            </datalist>
          </label>
          <Select
            label="Mode"
            value={draft.mode}
            options={[
              { value: "timed", label: "Timed" },
              { value: "looping", label: "Looping" },
            ]}
            onChange={(mode) => setDraft(setMode(draft, mode))}
          />
          <div className="ui-field">
            <span className="ui-label">Ads</span>
            <Checkbox checked={draft.ads} onChange={(ads) => patch({ ads })} label="Play ad breaks" />
          </div>
          <div className="ui-field full">
            <span className="ui-label">Logo</span>
            <div className="ui-line">
              {draftFolder(draft) !== "" && (
                <span className="ui-logo">
                  <IconImage src={`${mediaUrl(iconPath(draft))}?v=${iconStamp}`} />
                </span>
              )}
              <input className="ui-file-input" type="file" accept="image/png,image/jpeg,image/webp,image/gif,image/tiff,image/heif,image/avif" disabled={busy} onChange={(e) => void chooseIcon(e.target.files?.[0])} />
            </div>
            <span className="ui-muted">Saved right away as {draftFolder(draft) || "<folder>"}/channel.png</span>
          </div>
        </div>
        {iconError !== null && <Banner kind="error">{iconError}</Banner>}
        {errors.length > 0 && (
          <Banner kind="error">
            <ul>
              {errors.map((message) => (
                <li key={message}>{message}</li>
              ))}
            </ul>
          </Banner>
        )}
        <FailureBanner failure={failure} onReload={load} />
        {warnings.length > 0 && (
          <Banner kind="warn">
            <ul>
              {warnings.map((issue, i) => (
                <li key={i}>{issue.message}</li>
              ))}
            </ul>
          </Banner>
        )}
        <div className="ui-line">
          <PillButton kind="danger" disabled={busy} onClick={() => setDeleting(true)}>
            Delete channel
          </PillButton>
          <span className="grow" />
          <PillButton disabled={busy} onClick={() => original !== undefined && setDraft(draftFromChannel(original))}>
            Cancel
          </PillButton>
          <PillButton kind="primary" disabled={busy} onClick={() => void save()}>
            Save
          </PillButton>
        </div>
      </Panel>
      {deleting && original !== undefined && (
        <ConfirmDialog
          title="Delete channel"
          body={`Delete channel ${original.number} ${original.name}? Its files stay on the drive.`}
          confirm="Delete"
          danger
          onCancel={() => setDeleting(false)}
          onConfirm={() => {
            setDeleting(false);
            void persist(applyDelete(config, number), onDeleted);
          }}
        />
      )}
    </>
  );
}
