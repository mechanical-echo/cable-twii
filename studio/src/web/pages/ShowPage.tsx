import { mediaUrl } from "../api";
import { channelsAiring } from "../../server/scheduleRefs.js";
import { formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { AirsOnChips } from "../library/AirsOn";
import { formatEpisodeCode } from "../preview";
import { type Route, parentsOf } from "../router";
import { allShows, offPaths } from "../shows";
import { seasonLabel } from "../tree";
import { Banner, Crumbs, EmptyState, PageHead, Panel } from "../ui/layout";
import { Chip } from "../ui/status";
import { useLibraryData } from "../useLibraryData";

interface Props {
  channel: string;
  show: string;
  onNavigate: (route: Route) => void;
}

export function ShowPage({ channel, show: folder, onNavigate }: Props) {
  const { library, config, error } = useLibraryData();
  const crumbs = parentsOf({ page: "show", channel, show: folder }, () => null);
  if (error !== null) return <Banner kind="error">{error}</Banner>;
  if (library === null || config === null) return <EmptyState>Loading…</EmptyState>;
  const show = allShows(library).find((s) => s.channel === channel && s.folder === folder);
  if (show === undefined) {
    return (
      <section>
        <Crumbs items={crumbs} onNavigate={onNavigate} />
        <EmptyState>This show is not in the library any more.</EmptyState>
      </section>
    );
  }
  const off = offPaths(library);
  const seasons = [...new Set(show.episode.map((e) => e.season))].sort((a, b) => a - b);
  return (
    <section>
      <Crumbs items={crumbs} onNavigate={onNavigate} />
      <header className="ui-pagehead">
        <span className="ui-logo">{show.icon !== "" && <IconImage src={mediaUrl(show.icon)} />}</span>
        <div className="ui-pagehead-text">
          <h1>{show.name}</h1>
          <div className="ui-pagehead-sub">Stored in {channel}</div>
        </div>
      </header>
      <Panel title="Airs on">
        <AirsOnChips numbers={channelsAiring(config, { channel, show: folder })} config={config} onNavigate={onNavigate} />
      </Panel>
      {seasons.map((season) => (
        <Panel key={season} title={seasonLabel(season)}>
          {show.episode
            .filter((e) => e.season === season)
            .map((ep) => (
              <div key={ep.path} className="ui-ep link" role="button" tabIndex={0} onClick={() => onNavigate({ page: "episode", path: ep.path })} onKeyDown={(e) => e.key === "Enter" && onNavigate({ page: "episode", path: ep.path })}>
                <span className="ui-ep-code">{ep.season >= 1 ? formatEpisodeCode(ep.season, ep.episode) : ""}</span>
                <span className="grow">{ep.title}</span>
                <span className="ui-length">{formatDuration(ep.duration_ms)}</span>
                {off.has(ep.path) ? <Chip kind="warn">Off air</Chip> : <Chip kind="ok">On air</Chip>}
              </div>
            ))}
        </Panel>
      ))}
    </section>
  );
}
