import { useState } from "react";
import { mediaUrl } from "../api";
import { channelFolder, channelsAiring } from "../../server/scheduleRefs.js";
import { ShowInfoDialog } from "../channel/ShowInfoDialog";
import { FailureBanner } from "../FailureBanner";
import { formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { AirsOnChips } from "../library/AirsOn";
import { formatEpisodeCode } from "../preview";
import { type Route, parentsOf } from "../router";
import { allShows, offPaths, seasonTargets } from "../shows";
import { buildTree, seasonLabel } from "../tree";
import { Checkbox, PillButton, Select } from "../ui/controls";
import { Banner, Crumbs, EmptyState, Panel, rowLink } from "../ui/layout";
import { Chip } from "../ui/status";
import { RemovedList, useConfirm } from "../ui/useConfirm";
import { useLibraryData } from "../useLibraryData";
import { useLibraryEdit } from "../useLibraryEdit";

interface Props {
  channel: string;
  show: string;
  server: boolean;
  onNavigate: (route: Route) => void;
}

export function ShowPage({ channel, show: folder, server, onNavigate }: Props) {
  const { library, config, error, reload } = useLibraryData();
  const edit = useLibraryEdit(server);
  const [dialog, ask] = useConfirm();
  const [ticked, setTicked] = useState<Set<string>>(new Set());
  const [target, setTarget] = useState<string | null>(null);
  const [editing, setEditing] = useState(false);
  const crumbs = parentsOf({ page: "show", channel, show: folder }, () => null);
  if (error !== null) return <Banner kind="error">{error}</Banner>;
  if (library === null || config === null) return <EmptyState>Loading…</EmptyState>;
  const show = allShows(library).find((s) => s.channel === channel && s.folder === folder);
  if (show === undefined) {
    return (
      <section>
        <Crumbs items={crumbs} onNavigate={onNavigate} />
        {edit.notice !== null && <Banner kind="ok">{edit.notice}</Banner>}
        <EmptyState>This show is not in the library any more.</EmptyState>
      </section>
    );
  }
  const off = offPaths(library);
  const seasons = [...new Set(show.episode.map((e) => e.season))].sort((a, b) => a - b);
  const home = config.channel.find((c) => channelFolder(c) === channel);
  const channelName = (n: number) => config.channel.find((c) => c.number === n)?.name ?? `Channel ${n}`;
  const picked = show.episode.filter((e) => ticked.has(e.path)).map((e) => e.path);
  const targets = seasonTargets(show);
  const existing = new Set(show.episode.map((e) => e.season));
  const targetNow = target ?? String(targets[0]);

  const toggle = (paths: string[], on: boolean) =>
    setTicked((prev) => {
      const next = new Set(prev);
      for (const p of paths) {
        if (on) next.add(p);
        else next.delete(p);
      }
      return next;
    });

  async function after(result: unknown) {
    if (result === null) return;
    setTicked(new Set());
    reload();
  }

  const confirmRemoved = (title: string, confirm: string, danger = false, extra?: string) => (dry: { removed: { channel: number; label: string }[] }) =>
    dry.removed.length === 0 && !danger
      ? Promise.resolve(true)
      : ask({
          title,
          body: (
            <>
              <RemovedList removed={dry.removed} channelName={channelName} />
              {extra !== undefined && <p>{extra}</p>}
            </>
          ),
          confirm,
          danger,
        });

  async function deleteShow() {
    const result = await edit.run(
      { kind: "delete-show", channel, show: folder },
      { done: "Deleted.", confirm: confirmRemoved(`Delete ${show!.name} and all ${show!.episode.length} episodes?`, "Delete show", true, "Delete can't be undone.") },
    );
    if (result !== null) onNavigate({ page: "shows" });
  }

  return (
    <section>
      <Crumbs items={crumbs} onNavigate={onNavigate} />
      <header className="ui-pagehead">
        <span className="ui-logo">{show.icon !== "" && <IconImage src={mediaUrl(show.icon)} />}</span>
        <div className="ui-pagehead-text">
          <h1>{show.name}</h1>
          <div className="ui-pagehead-sub">Stored in {channel}</div>
        </div>
        <div className="ui-pagehead-actions">
          <PillButton kind="primary" onClick={() => onNavigate({ page: "add", to: "episodes", channel: home?.number ?? null })}>
            Add episodes
          </PillButton>
          <PillButton onClick={() => setEditing(true)}>Edit info</PillButton>
          <PillButton kind="danger" disabled={edit.busy} onClick={() => void deleteShow()}>
            Delete show
          </PillButton>
        </div>
      </header>
      {edit.notice !== null && <Banner kind="ok">{edit.notice}</Banner>}
      <FailureBanner failure={edit.failure} onReload={reload} />
      <Panel title="Airs on">
        <AirsOnChips numbers={channelsAiring(config, { channel, show: folder })} config={config} onNavigate={onNavigate} />
      </Panel>
      {picked.length > 0 && (
        <div className="ui-bulk">
          <span className="ui-row-name">{picked.length} ticked</span>
          <Select
            label="Move to"
            value={targetNow}
            options={targets.map((n) => ({ value: String(n), label: existing.has(n) ? `Season ${n}` : `New season ${n}` }))}
            onChange={setTarget}
          />
          <PillButton small disabled={edit.busy} onClick={() => void edit.run({ kind: "move-season", paths: picked, season: Number(targetNow) }, { done: "Moved." }).then(after)}>
            Move
          </PillButton>
          <span className="grow" />
          <PillButton small disabled={edit.busy} onClick={() => void edit.run({ kind: "off", paths: picked, off: true }, { done: "Switched off.", confirm: confirmRemoved("Switch these off?", "Switch off") }).then(after)}>
            Switch off
          </PillButton>
          <PillButton small disabled={edit.busy} onClick={() => void edit.run({ kind: "off", paths: picked, off: false }, { done: "Back on air." }).then(after)}>
            Switch on
          </PillButton>
          <PillButton
            small
            kind="danger"
            disabled={edit.busy}
            onClick={() =>
              void edit.run({ kind: "delete", paths: picked }, { done: "Deleted.", confirm: confirmRemoved(`Delete ${picked.length} episodes?`, "Delete", true, "Delete can't be undone.") }).then(after)
            }
          >
            Delete
          </PillButton>
        </div>
      )}
      {seasons.map((season) => {
        const episodes = show.episode.filter((e) => e.season === season);
        const all = episodes.every((e) => ticked.has(e.path));
        return (
          <Panel key={season} title={seasonLabel(season)} actions={<Checkbox checked={all} label="Tick all" onChange={(on) => toggle(episodes.map((e) => e.path), on)} />}>
            {episodes.map((ep) => (
              <div key={ep.path} className={off.has(ep.path) ? "ui-ep off" : "ui-ep"}>
                <Checkbox checked={ticked.has(ep.path)} ariaLabel={`Tick ${ep.title}`} onChange={(on) => toggle([ep.path], on)} />
                <span {...rowLink("ui-ep-main", () => onNavigate({ page: "episode", path: ep.path }))}>
                  <span className="ui-ep-code">{ep.season >= 1 ? formatEpisodeCode(ep.season, ep.episode) : ""}</span>
                  <span className="grow">{ep.title}</span>
                  <span className="ui-length">{formatDuration(ep.duration_ms)}</span>
                  {off.has(ep.path) ? <Chip kind="warn">Off air</Chip> : <Chip kind="ok">On air</Chip>}
                </span>
              </div>
            ))}
          </Panel>
        );
      })}
      {editing && (
        <ShowInfoDialog
          node={buildTree({ ...library, show: [show], ad: [] })[0]!.shows[0]!}
          channelFolder={channel}
          onClose={() => setEditing(false)}
          onSaved={() => reload()}
        />
      )}
      {dialog}
    </section>
  );
}
