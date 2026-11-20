import { useCallback, useEffect, useState } from "react";
import { type Config, getConfig, getLibrary, mediaUrl } from "../api";
import { ScheduleTab } from "../channel/ScheduleTab";
import { SettingsTab } from "../channel/SettingsTab";
import { TodayTab } from "../channel/TodayTab";
import { ShowsTab } from "../channel/ShowsTab";
import { effectiveFolder, iconPath } from "../channels";
import { IconImage } from "../IconImage";
import { type ChannelTab, type Route, parentsOf } from "../router";
import type { WebLibrary } from "../tree";
import { PillButton } from "../ui/controls";
import { Banner, Crumbs, EmptyState } from "../ui/layout";
import { Tabs } from "../ui/Tabs";

interface Props {
  number: number;
  tab: ChannelTab;
  onNavigate: (route: Route) => void;
  onDirty: (dirty: boolean) => void;
}

const TABS: { id: ChannelTab; label: string }[] = [
  { id: "today", label: "Today" },
  { id: "schedule", label: "Schedule" },
  { id: "shows", label: "Shows" },
  { id: "settings", label: "Settings" },
];

export function ChannelPage({ number, tab, onNavigate, onDirty }: Props) {
  const [config, setConfig] = useState<Config | null>(null);
  const [library, setLibrary] = useState<WebLibrary | null>(null);
  const [error, setError] = useState<string | null>(null);

  const load = useCallback(() => {
    setError(null);
    getConfig()
      .then((result) => setConfig(result.config))
      .catch((err: Error) => setError(err.message));
    getLibrary()
      .then(setLibrary)
      .catch(() => setLibrary(null));
  }, []);

  useEffect(load, [load]);

  if (error !== null) {
    return (
      <Banner kind="error">
        <span className="grow">{error}</span>
        <PillButton small onClick={load}>
          Retry
        </PillButton>
      </Banner>
    );
  }
  if (config === null) return <EmptyState>Loading…</EmptyState>;
  const channel = config.channel.find((c) => c.number === number);
  if (channel === undefined) {
    return (
      <EmptyState>
        No channel {number}.{" "}
        <a
          href="#/channels"
          onClick={(e) => {
            e.preventDefault();
            onNavigate({ page: "channels" });
          }}
        >
          See all channels
        </a>
      </EmptyState>
    );
  }

  const facts = [channel.category, channel.mode === "timed" ? "Timed" : "Looping", channel.ads ? "ads on" : "ads off"].filter((f) => f !== "");
  return (
    <section>
      <Crumbs items={parentsOf({ page: "channel", number, tab }, () => null)} onNavigate={onNavigate} />
      <header className="ui-pagehead">
        <span className="ui-num">{channel.number}</span>
        <span className="ui-logo">
          <IconImage src={mediaUrl(iconPath(channel))} />
        </span>
        <div className="ui-pagehead-text">
          <h1>{channel.name}</h1>
          <div className="ui-pagehead-sub">{facts.join(" · ")}</div>
        </div>
        <div className="ui-pagehead-actions">
          <PillButton kind="primary" onClick={() => onNavigate({ page: "add", to: "episodes", channel: channel.number })}>
            Add episodes
          </PillButton>
        </div>
      </header>
      <Tabs tabs={TABS} active={tab} onChange={(next) => onNavigate({ page: "channel", number, tab: next })} />
      {tab === "today" && <TodayTab key={number} channel={channel} library={library} />}
      {tab === "schedule" && <ScheduleTab number={number} onDirty={onDirty} />}
      {tab === "shows" && <ShowsTab folder={effectiveFolder(channel)} onAdd={() => onNavigate({ page: "add", to: "episodes", channel: channel.number })} />}
      {tab === "settings" && (
        <SettingsTab
          number={number}
          onDeleted={() => onNavigate({ page: "channels" })}
          onSaved={(saved) => (saved === number ? load() : onNavigate({ page: "channel", number: saved, tab: "settings" }))}
        />
      )}
    </section>
  );
}
