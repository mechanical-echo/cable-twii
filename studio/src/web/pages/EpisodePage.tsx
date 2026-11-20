import { useEffect, useState } from "react";
import { getFileFacts, getShowMeta, mediaUrl, putShowMeta } from "../api";
import { FailureBanner } from "../FailureBanner";
import { formatBytes, formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { AirsOnChips } from "../library/AirsOn";
import { formatEpisodeCode } from "../preview";
import { type Route, parentsOf } from "../router";
import { episodeChannels } from "../../server/scheduleRefs.js";
import { allShows, findEpisode } from "../shows";
import { descriptionKey } from "../tree";
import { Checkbox, PillButton, TextField } from "../ui/controls";
import { Banner, Crumbs, EmptyState, PageHead, Panel } from "../ui/layout";
import { MpegPlayer } from "../ui/MpegPlayer";
import { RemovedList, useConfirm } from "../ui/useConfirm";
import { useLibraryData } from "../useLibraryData";
import { useLibraryEdit } from "../useLibraryEdit";

interface Facts {
  size: number;
  mtimeMs: number;
  source: string | null;
}

export function sourceLabel(source: string | null): string {
  if (source === null) return "Not known";
  return source.startsWith("upload:") ? `Uploaded as ${source.slice("upload:".length)}` : source;
}

export function useFacts(path: string): Facts | null {
  const [facts, setFacts] = useState<Facts | null>(null);
  useEffect(() => {
    setFacts(null);
    getFileFacts(path)
      .then(setFacts)
      .catch(() => setFacts(null));
  }, [path]);
  return facts;
}

export function FactList({ durationMs, facts, path }: { durationMs: number; facts: Facts | null; path: string }) {
  return (
    <dl className="ui-facts">
      <dt>Length</dt>
      <dd>{formatDuration(durationMs)}</dd>
      <dt>Size</dt>
      <dd>{facts === null ? "…" : formatBytes(facts.size)}</dd>
      <dt>File</dt>
      <dd>{path}</dd>
      <dt>Converted from</dt>
      <dd>{facts === null ? "…" : sourceLabel(facts.source)}</dd>
      <dt>Converted</dt>
      <dd>{facts === null ? "…" : new Date(facts.mtimeMs).toLocaleString()}</dd>
    </dl>
  );
}

function whole(text: string): number | null {
  return /^\d+$/.test(text.trim()) ? Number(text.trim()) : null;
}

export function EpisodePage({ path, server, onNavigate }: { path: string; server: boolean; onNavigate: (route: Route) => void }) {
  const { library, config, error, reload } = useLibraryData();
  const facts = useFacts(path);
  const edit = useLibraryEdit(server);
  const [dialog, ask] = useConfirm();
  const [title, setTitle] = useState<string | null>(null);
  const [season, setSeason] = useState<string | null>(null);
  const [number, setNumber] = useState<string | null>(null);
  const [description, setDescription] = useState<string | null>(null);
  if (error !== null) return <Banner kind="error">{error}</Banner>;
  if (library === null || config === null) return <EmptyState>Loading…</EmptyState>;
  const showName = (ref: { channel: string; show: string }) => allShows(library).find((s) => s.channel === ref.channel && s.folder === ref.show)?.name ?? null;
  const channelName = (n: number) => config.channel.find((c) => c.number === n)?.name ?? `Channel ${n}`;
  const crumbs = parentsOf({ page: "episode", path }, (n) => config.channel.find((c) => c.number === n)?.name ?? null, showName);
  const found = findEpisode(library, path);
  if (found === null) {
    return (
      <section>
        <Crumbs items={crumbs} onNavigate={onNavigate} />
        {edit.notice !== null && <Banner kind="ok">{edit.notice}</Banner>}
        <EmptyState>This episode is not in the library any more.</EmptyState>
      </section>
    );
  }
  const { show, episode, off } = found;
  const seasoned = episode.season >= 1;
  const code = formatEpisodeCode(seasoned ? episode.season : null, episode.episode);
  const ref = { channel: show.channel, show: show.folder };
  const titleNow = title ?? episode.title;
  const seasonNow = season ?? String(episode.season);
  const numberNow = number ?? String(episode.episode);
  const descriptionNow = description ?? episode.description;
  const renamed = titleNow.trim() !== episode.title || (seasoned && (seasonNow !== String(episode.season) || numberNow !== String(episode.episode)));
  const changed = renamed || descriptionNow !== episode.description;

  async function save() {
    let target = path;
    if (renamed) {
      const result = await edit.run({ kind: "episode", path, title: titleNow, season: seasoned ? whole(seasonNow) : null, episode: seasoned ? whole(numberNow) : null });
      if (result === null) return;
      target = result.moves.find((m) => m.from === path)?.to ?? path;
    }
    if (descriptionNow !== episode.description) {
      const meta = await getShowMeta(show.channel, show.folder);
      const key = descriptionKey({ season: seasoned ? whole(seasonNow) ?? episode.season : 0, episode: whole(numberNow) ?? episode.episode, path: target });
      const episodes = { ...meta.meta.episodes };
      if (descriptionNow.trim() === "") delete episodes[key];
      else episodes[key] = descriptionNow.trim();
      await putShowMeta({ channelFolder: show.channel, showFolder: show.folder, name: meta.meta.name ?? show.name, episodes, mtimeMs: meta.mtimeMs });
    }
    setTitle(null);
    setSeason(null);
    setNumber(null);
    setDescription(null);
    if (target !== path) onNavigate({ page: "episode", path: target });
    else reload();
  }

  async function switchAir(on: boolean) {
    const result = on
      ? await edit.run({ kind: "off", paths: [path], off: false }, { done: "Back on air." })
      : await edit.run(
          { kind: "off", paths: [path], off: true },
          {
            done: "Switched off.",
            confirm: (dry) =>
              dry.removed.length === 0
                ? Promise.resolve(true)
                : ask({ title: "Switch off this episode?", body: <RemovedList removed={dry.removed} channelName={channelName} />, confirm: "Switch off" }),
          },
        );
    if (result !== null) reload();
  }

  async function remove() {
    const result = await edit.run(
      { kind: "delete", paths: [path] },
      {
        done: "Deleted.",
        confirm: (dry) =>
          ask({
            title: `Delete ${episode.title}?`,
            body: (
              <>
                <p>{path}</p>
                <RemovedList removed={dry.removed} channelName={channelName} />
                <p>Delete can't be undone.</p>
              </>
            ),
            confirm: "Delete",
            danger: true,
          }),
      },
    );
    if (result !== null) onNavigate({ page: "show", channel: show.channel, show: show.folder });
  }

  return (
    <section>
      <Crumbs items={crumbs} onNavigate={onNavigate} />
      <PageHead
        title={seasoned ? `${code} · ${episode.title}` : episode.title}
        sub={
          <span className="ui-line">
            {show.icon !== "" && (
              <span className="ui-logo small">
                <IconImage src={mediaUrl(show.icon)} />
              </span>
            )}
            {show.name}
          </span>
        }
        actions={<Checkbox checked={!off} disabled={edit.busy} onChange={(on) => void switchAir(on)} label={off ? "Off air" : "On air"} />}
      />
      {edit.notice !== null && <Banner kind="ok">{edit.notice}</Banner>}
      <FailureBanner failure={edit.failure} onReload={reload} />
      <MpegPlayer key={path} path={path} durationMs={episode.duration_ms} aspect="16:9" />
      <Panel title="Airs on">
        <AirsOnChips numbers={episodeChannels(config, ref, episode.season, episode.episode)} config={config} onNavigate={onNavigate} off={off} />
      </Panel>
      <Panel title="Information">
        <div className="ui-settings">
          <TextField label="Title" value={titleNow} onChange={setTitle} width="full" />
          {seasoned && (
            <div className="ui-line">
              <TextField label="Season" value={seasonNow} onChange={setSeason} width="short" />
              <TextField label="Episode" value={numberNow} onChange={setNumber} width="short" />
            </div>
          )}
          <TextField label="Description" value={descriptionNow} onChange={setDescription} placeholder="Shown on the Wii" width="full" />
        </div>
        <div className="ui-line">
          <PillButton kind="danger" disabled={edit.busy} onClick={() => void remove()}>
            Delete episode
          </PillButton>
          <span className="grow" />
          <PillButton disabled={!changed || edit.busy} onClick={() => void save()} kind="primary">
            Save
          </PillButton>
        </div>
      </Panel>
      <Panel title="File">
        <FactList durationMs={episode.duration_ms} facts={facts} path={path} />
      </Panel>
      {dialog}
    </section>
  );
}
