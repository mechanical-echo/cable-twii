import { mediaUrl } from "../api";
import { IconImage } from "../IconImage";
import { AirsOnChips } from "../library/AirsOn";
import type { Route } from "../router";
import { showGroups } from "../shows";
import { PillButton } from "../ui/controls";
import { Banner, EmptyState, ListRow, PageHead, Panel } from "../ui/layout";
import { Chip } from "../ui/status";
import { useLibraryData } from "../useLibraryData";

export function ShowsPage({ onNavigate }: { onNavigate: (route: Route) => void }) {
  const { library, config, error } = useLibraryData();
  const groups = library !== null && config !== null ? showGroups(library, config) : [];
  return (
    <section>
      <PageHead
        title="Shows"
        actions={
          <PillButton kind="primary" onClick={() => onNavigate({ page: "add", to: "episodes", channel: null })}>
            Add episodes
          </PillButton>
        }
      />
      {error !== null && <Banner kind="error">{error}</Banner>}
      {(library === null || config === null) && error === null && <EmptyState>Loading…</EmptyState>}
      {library !== null && config !== null && groups.length === 0 && <EmptyState>No shows yet. Add some episodes to get started.</EmptyState>}
      {config !== null &&
        groups.map((group) => (
          <Panel key={group.folder} title={group.label}>
            {group.shows.map((show) => (
              <ListRow key={`${show.ref.channel}/${show.ref.show}`} onClick={() => onNavigate({ page: "show", channel: show.ref.channel, show: show.ref.show })}>
                <span className="ui-logo">{show.icon !== "" && <IconImage src={mediaUrl(show.icon)} />}</span>
                <div className="grow">
                  <div className="ui-row-name">{show.name}</div>
                  <div className="ui-row-sub">
                    {show.seasons} {show.seasons === 1 ? "season" : "seasons"} · {show.episodes} {show.episodes === 1 ? "episode" : "episodes"}
                  </div>
                </div>
                {show.off > 0 && <Chip kind="warn">{show.off} off air</Chip>}
                <AirsOnChips numbers={show.airsOn} config={config} onNavigate={onNavigate} />
              </ListRow>
            ))}
          </Panel>
        ))}
    </section>
  );
}
