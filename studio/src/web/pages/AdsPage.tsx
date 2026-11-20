import { useEffect, useState } from "react";
import { getConfig, getLibrary, getNotes } from "../api";
import { type AdGroup, adGroups, skippedAds } from "../ads";
import { formatDuration } from "../format";
import type { Route } from "../router";
import { offPaths } from "../shows";
import { PillButton } from "../ui/controls";
import { Banner, EmptyState, PageHead, Panel, rowLink } from "../ui/layout";
import { Chip } from "../ui/status";

interface Props {
  onNavigate: (route: Route) => void;
}

export function AdsPage({ onNavigate }: Props) {
  const [groups, setGroups] = useState<AdGroup[] | null>(null);
  const [skipped, setSkipped] = useState<{ path: string; reason: string }[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [off, setOff] = useState<Set<string>>(new Set());

  useEffect(() => {
    Promise.all([getLibrary(), getConfig()])
      .then(([library, { config }]) => {
        setOff(offPaths(library));
        setGroups(adGroups({ ...library, ad: [...library.ad, ...(library.off?.ad ?? [])] }, config));
      })
      .catch((err: Error) => setError(err.message));
    getNotes()
      .then((notes) => setSkipped(skippedAds(notes)))
      .catch(() => setSkipped([]));
  }, []);

  return (
    <section>
      <PageHead
        title="Ads"
        sub="Ads play in the breaks of channels that have ads on."
        actions={
          <PillButton kind="primary" onClick={() => onNavigate({ page: "add", to: "ads", channel: null })}>
            Add ads
          </PillButton>
        }
      />
      {error !== null && <Banner kind="error">{error}</Banner>}
      {groups === null && error === null && <EmptyState>Loading…</EmptyState>}
      {groups !== null && groups.length === 0 && <EmptyState>No ads yet. Add some to fill the breaks.</EmptyState>}
      {groups?.map((group) => (
        <Panel key={group.channel} title={group.label}>
          {group.ads.map((ad) => (
            <div key={ad.path} {...rowLink(off.has(ad.path) ? "ui-ep off" : "ui-ep", () => onNavigate({ page: "ad", path: ad.path }))}>
              <span className="grow">{ad.name}</span>
              {off.has(ad.path) && <Chip kind="warn">Off air</Chip>}
              <span className="ui-length">{formatDuration(ad.durationMs)}</span>
            </div>
          ))}
        </Panel>
      ))}
      {skipped.length > 0 && (
        <div className="ui-gap">
          <Panel title="Skipped">
            <p className="ui-muted">These files are in an ads folder but don't play.</p>
            {skipped.map((s) => (
              <div key={s.path} className="ui-ep">
                <span className="grow">{s.path}</span>
                <span className="ui-muted">{s.reason}</span>
              </div>
            ))}
          </Panel>
        </div>
      )}
    </section>
  );
}
