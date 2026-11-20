import { useCallback, useEffect, useState } from "react";
import { type Config, getConfig, getLibrary, getPreview, mediaUrl, putConfig, validate } from "../api";
import { type ChannelSummary, channelSummaries, nowLine, orphanFolders } from "../channelList";
import { applyEdit, newDraft, setMode, validateDraft } from "../channels";
import { FailureBanner } from "../FailureBanner";
import { type Failure, toFailure } from "../failure";
import { IconImage } from "../IconImage";
import { localDate, localMs } from "../preview";
import type { Route } from "../router";
import { PillButton, Select, TextField } from "../ui/controls";
import { Banner, EmptyState, ListRow, PageHead, Panel } from "../ui/layout";
import { Chip } from "../ui/status";

interface Props {
  onNavigate: (route: Route) => void;
}

function AddChannelDialog({ config, mtimeMs, onAdded, onCancel }: { config: Config; mtimeMs: number; onAdded: (number: number) => void; onCancel: () => void }) {
  const [draft, setDraft] = useState(() => newDraft(config.channel));
  const [errors, setErrors] = useState<string[]>([]);
  const [failure, setFailure] = useState<Failure | null>(null);
  const [busy, setBusy] = useState(false);

  async function save() {
    const problems = validateDraft(draft, config.channel, null);
    setErrors(problems);
    if (problems.length > 0) return;
    setBusy(true);
    setFailure(null);
    try {
      await putConfig(applyEdit(config, null, draft), mtimeMs);
      onAdded(Number(draft.number));
    } catch (err) {
      setFailure(toFailure(err));
      setBusy(false);
    }
  }

  return (
    <div className="overlay">
      <form
        className="ui-dialog"
        role="dialog"
        aria-modal="true"
        aria-label="Add channel"
        onSubmit={(e) => {
          e.preventDefault();
          void save();
        }}
      >
        <h2>Add channel</h2>
        <div className="ui-stack">
          <div className="ui-line">
            <TextField label="Number" value={draft.number} onChange={(number) => setDraft({ ...draft, number })} width="short" />
            <Select
              label="Mode"
              value={draft.mode}
              options={[
                { value: "timed", label: "Timed" },
                { value: "looping", label: "Looping" },
              ]}
              onChange={(mode) => setDraft(setMode(draft, mode))}
            />
          </div>
          <TextField label="Name" value={draft.name} onChange={(name) => setDraft({ ...draft, name })} width="full" />
          <TextField label="Category" value={draft.category} onChange={(category) => setDraft({ ...draft, category })} placeholder="Kids, Music, Movies…" width="full" />
          {errors.length > 0 && (
            <Banner kind="error">
              <ul>
                {errors.map((e) => (
                  <li key={e}>{e}</li>
                ))}
              </ul>
            </Banner>
          )}
          <FailureBanner failure={failure} onReload={onCancel} />
        </div>
        <div className="ui-dialog-actions">
          <PillButton onClick={onCancel}>Cancel</PillButton>
          <PillButton kind="primary" type="submit" disabled={busy}>
            Add channel
          </PillButton>
        </div>
      </form>
    </div>
  );
}

function ChannelRow({ summary, onOpen }: { summary: ChannelSummary; onOpen: () => void }) {
  const noShows = summary.warnings[0] === "No shows yet";
  return (
    <ListRow onClick={onOpen}>
      <span className="ui-num">{summary.number}</span>
      <span className="ui-logo">
        <IconImage src={mediaUrl(summary.icon)} />
      </span>
      <div className="grow">
        <div className="ui-row-name">{summary.name}</div>
        <div className="ui-row-sub">{summary.now !== null ? <>Now <b>{summary.now}</b></> : noShows ? "Nothing to show yet" : "Nothing on air right now"}</div>
      </div>
      <Chip>{summary.mode === "timed" ? "Timed" : "Looping"}</Chip>
      {!noShows && (
        <Chip>
          {summary.shows} {summary.shows === 1 ? "show" : "shows"} · {summary.episodes} {summary.episodes === 1 ? "episode" : "episodes"}
        </Chip>
      )}
      {summary.warnings.map((w) => (
        <Chip key={w} kind={w.startsWith("Error") ? "error" : "warn"}>
          {w === "No shows yet" ? w : w.startsWith("Error") ? "Has errors" : "Has warnings"}
        </Chip>
      ))}
    </ListRow>
  );
}

export function ChannelListPage({ onNavigate }: Props) {
  const [summaries, setSummaries] = useState<ChannelSummary[] | null>(null);
  const [orphans, setOrphans] = useState<string[]>([]);
  const [config, setConfig] = useState<{ config: Config; mtimeMs: number } | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [adding, setAdding] = useState(false);

  const load = useCallback(() => {
    setError(null);
    Promise.all([getConfig(), getLibrary(), validate().catch(() => ({ ok: true, issues: [] }))])
      .then(async ([loaded, library, checked]) => {
        setConfig(loaded);
        setOrphans(orphanFolders(loaded.config, library));
        const base = channelSummaries(loaded.config, library, checked.issues);
        setSummaries(base);
        const now = new Date();
        const lines = await Promise.all(base.map((s) => getPreview(s.number, localDate(now)).then((lineup) => nowLine(lineup, localMs(now)), () => null)));
        setSummaries(base.map((s, i) => ({ ...s, now: lines[i] ?? null })));
      })
      .catch((err: Error) => setError(err.message));
  }, []);

  useEffect(load, [load]);

  return (
    <section>
      <PageHead
        title="Channels"
        actions={
          <PillButton disabled={config === null} onClick={() => setAdding(true)}>
            Add channel
          </PillButton>
        }
      />
      {error !== null && (
        <Banner kind="error">
          <span className="grow">{error}</span>
          <PillButton small onClick={load}>
            Retry
          </PillButton>
        </Banner>
      )}
      {summaries === null && error === null && <EmptyState>Loading…</EmptyState>}
      {summaries !== null && summaries.length === 0 && <EmptyState>No channels yet. Add one to get started.</EmptyState>}
      {summaries?.map((s) => (
        <ChannelRow key={s.number} summary={s} onOpen={() => onNavigate({ page: "channel", number: s.number, tab: "today" })} />
      ))}
      {orphans.length > 0 && (
        <div className="ui-gap">
          <Panel title="Not on a channel">
            <p className="ui-muted">These folders have shows but no channel uses them. Give a channel this folder in its Settings to put them on air.</p>
            {orphans.map((folder) => (
              <ListRow key={folder} onClick={() => onNavigate({ page: "folder", folder })}>
                <span className="grow ui-row-name">{folder}</span>
              </ListRow>
            ))}
          </Panel>
        </div>
      )}
      {adding && config !== null && (
        <AddChannelDialog
          config={config.config}
          mtimeMs={config.mtimeMs}
          onCancel={() => setAdding(false)}
          onAdded={(number) => onNavigate({ page: "channel", number, tab: "settings" })}
        />
      )}
    </section>
  );
}
