import { AirsOnChips } from "../library/AirsOn";
import { type Route, parentsOf } from "../router";
import { adChannels, channelFolder } from "../../server/scheduleRefs.js";
import { findAd } from "../shows";
import { Banner, Crumbs, EmptyState, PageHead, Panel } from "../ui/layout";
import { MpegPlayer } from "../ui/MpegPlayer";
import { useLibraryData } from "../useLibraryData";
import { FactList, useFacts } from "./EpisodePage";

export function AdPage({ path, onNavigate }: { path: string; onNavigate: (route: Route) => void }) {
  const { library, config, error } = useLibraryData();
  const facts = useFacts(path);
  if (error !== null) return <Banner kind="error">{error}</Banner>;
  if (library === null || config === null) return <EmptyState>Loading…</EmptyState>;
  const crumbs = parentsOf({ page: "ad", path }, () => null);
  const found = findAd(library, path);
  if (found === null) {
    return (
      <section>
        <Crumbs items={crumbs} onNavigate={onNavigate} />
        <EmptyState>This ad is not in the library any more.</EmptyState>
      </section>
    );
  }
  const { ad, off } = found;
  const where = ad.channel === "" ? "All channels" : (config.channel.find((c) => channelFolder(c) === ad.channel)?.name ?? ad.channel);
  return (
    <section>
      <Crumbs items={crumbs} onNavigate={onNavigate} />
      <PageHead title={path.slice(path.lastIndexOf("/") + 1).replace(/\.mpg$/i, "")} sub={`Ad · ${where}`} />
      <MpegPlayer key={path} path={path} durationMs={ad.duration_ms} aspect="16:9" />
      <Panel title="Airs on">
        <AirsOnChips numbers={adChannels(config, library, path)} config={config} onNavigate={onNavigate} off={off} />
      </Panel>
      <Panel title="Information">
        <FactList durationMs={ad.duration_ms} facts={facts} path={path} />
      </Panel>
    </section>
  );
}
