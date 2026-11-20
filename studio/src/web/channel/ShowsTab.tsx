import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { getLibrary, mediaUrl } from "../api";
import { type EpisodeStatus, episodeStatus } from "../channelList";
import { formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { formatEpisodeCode } from "../preview";
import { type ShowNode, type WebLibrary, buildTree, seasonLabel } from "../tree";
import { PillButton } from "../ui/controls";
import { Banner, EmptyState, Panel } from "../ui/layout";
import { Chip } from "../ui/status";
import { useJobs } from "../useJobs";
import { ShowInfoDialog } from "./ShowInfoDialog";

interface Props {
  folder: string;
  onAdd?: () => void;
}

const REFRESH_DELAY_MS = 2600;

function StatusChip({ status }: { status: EpisodeStatus }) {
  if (status.state === "running") return <Chip>Converting {Math.round(status.progress * 100)}%</Chip>;
  if (status.state === "queued") return <Chip>Queued</Chip>;
  if (status.state === "failed") return <Chip kind="error">Failed</Chip>;
  return <Chip kind="ok">On air</Chip>;
}

export function ShowsTab({ folder, onAdd }: Props) {
  const [library, setLibrary] = useState<WebLibrary | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [editing, setEditing] = useState<ShowNode | null>(null);
  const [stamp, setStamp] = useState(0);
  const timer = useRef<ReturnType<typeof setTimeout>>();

  const load = useCallback(() => {
    getLibrary()
      .then(setLibrary)
      .catch((err: Error) => setError(err.message));
  }, []);

  useEffect(() => {
    load();
    return () => clearTimeout(timer.current);
  }, [load]);

  const { jobs } = useJobs(() => {
    clearTimeout(timer.current);
    timer.current = setTimeout(load, REFRESH_DELAY_MS);
  });

  const shows = useMemo(() => (library === null ? [] : (buildTree(library).find((c) => c.channel === folder)?.shows ?? [])), [library, folder]);

  return (
    <>
      {error !== null && <Banner kind="error">{error}</Banner>}
      {library === null && error === null && <EmptyState>Loading…</EmptyState>}
      {library !== null && shows.length === 0 && (
        <Panel>
          <EmptyState>
            <p>No shows yet.</p>
            {onAdd !== undefined && (
              <PillButton kind="primary" onClick={onAdd}>
                Add episodes
              </PillButton>
            )}
          </EmptyState>
        </Panel>
      )}
      {shows.map((node) => (
        <Panel
          key={node.show.folder}
          title={node.show.name}
          actions={
            <>
              {node.show.icon !== "" && (
                <span className="ui-logo">
                  <IconImage src={`${mediaUrl(node.show.icon)}?v=${stamp}`} />
                </span>
              )}
              <PillButton small onClick={() => setEditing(node)}>
                Edit info
              </PillButton>
            </>
          }
        >
          {node.seasons.map((s) => (
            <div key={s.season} className="ui-season">
              <h3 className="ui-subhead">{seasonLabel(s.season)}</h3>
              {s.episodes.map((ep) => (
                <div key={ep.path} className="ui-ep">
                  <span className="ui-ep-code">{ep.season >= 1 ? formatEpisodeCode(ep.season, ep.episode) : ""}</span>
                  <span className="grow">{ep.title}</span>
                  <span className="ui-length">{formatDuration(ep.duration_ms)}</span>
                  <StatusChip status={episodeStatus(ep.path, jobs)} />
                </div>
              ))}
            </div>
          ))}
        </Panel>
      ))}
      {editing !== null && (
        <ShowInfoDialog
          node={editing}
          channelFolder={folder}
          onClose={() => setEditing(null)}
          onSaved={(next, iconChanged) => {
            setLibrary(next);
            if (iconChanged) setStamp(Date.now());
          }}
        />
      )}
    </>
  );
}
