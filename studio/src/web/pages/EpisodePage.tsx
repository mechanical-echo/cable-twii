import { useEffect, useState } from "react";
import { getFileFacts, mediaUrl } from "../api";
import { formatBytes, formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { AirsOnChips } from "../library/AirsOn";
import { formatEpisodeCode } from "../preview";
import { type Route, parentsOf } from "../router";
import { episodeChannels } from "../../server/scheduleRefs.js";
import { allShows, findEpisode } from "../shows";
import { Banner, Crumbs, EmptyState, PageHead, Panel } from "../ui/layout";
import { MpegPlayer } from "../ui/MpegPlayer";
import { useLibraryData } from "../useLibraryData";

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

export function EpisodePage({ path, onNavigate }: { path: string; onNavigate: (route: Route) => void }) {
  const { library, config, error } = useLibraryData();
  const facts = useFacts(path);
  if (error !== null) return <Banner kind="error">{error}</Banner>;
  if (library === null || config === null) return <EmptyState>Loading…</EmptyState>;
  const showName = (ref: { channel: string; show: string }) => allShows(library).find((s) => s.channel === ref.channel && s.folder === ref.show)?.name ?? null;
  const crumbs = parentsOf({ page: "episode", path }, (n) => config.channel.find((c) => c.number === n)?.name ?? null, showName);
  const found = findEpisode(library, path);
  if (found === null) {
    return (
      <section>
        <Crumbs items={crumbs} onNavigate={onNavigate} />
        <EmptyState>This episode is not in the library any more.</EmptyState>
      </section>
    );
  }
  const { show, episode, off } = found;
  const code = formatEpisodeCode(episode.season >= 1 ? episode.season : null, episode.episode);
  const ref = { channel: show.channel, show: show.folder };
  return (
    <section>
      <Crumbs items={crumbs} onNavigate={onNavigate} />
      <PageHead
        title={episode.season >= 1 ? `${code} · ${episode.title}` : episode.title}
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
      />
      <MpegPlayer key={path} path={path} durationMs={episode.duration_ms} aspect="16:9" />
      <Panel title="Airs on">
        <AirsOnChips numbers={episodeChannels(config, ref, episode.season, episode.episode)} config={config} onNavigate={onNavigate} off={off} />
      </Panel>
      <Panel title="Information">
        {episode.description !== "" && <p>{episode.description}</p>}
        <FactList durationMs={episode.duration_ms} facts={facts} path={path} />
      </Panel>
    </section>
  );
}
