import { useCallback, useEffect, useMemo, useState } from "react";
import { type Config, type LineupResult, getConfig, getLibrary, getPreview, mediaUrl } from "../api";
import { effectiveFolder, sortChannels } from "../channels";
import { FailureBanner } from "../FailureBanner";
import { type Failure, toFailure } from "../failure";
import { formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { type GuideRow, adBreakLabel, findShow, formatClock, formatEpisodeCode, groupAirings, localDate, localMs, nowPosition } from "../preview";
import type { WebLibrary } from "../tree";

const MINUTE_MS = 60_000;

export function PreviewPage() {
  const [config, setConfig] = useState<Config | null>(null);
  const [library, setLibrary] = useState<WebLibrary | null>(null);
  const [channel, setChannel] = useState<number | null>(null);
  const [date, setDate] = useState(() => localDate(new Date()));
  const [lineup, setLineup] = useState<LineupResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState<Failure | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [clock, setClock] = useState(() => new Date());

  const loadSetup = useCallback(() => {
    setLoadError(null);
    getConfig()
      .then((result) => {
        const sorted = sortChannels(result.config.channel);
        setConfig(result.config);
        setChannel((current) => (current !== null && sorted.some((c) => c.number === current) ? current : (sorted[0]?.number ?? null)));
      })
      .catch((err: Error) => setLoadError(err.message));
    getLibrary()
      .then(setLibrary)
      .catch(() => setLibrary(null));
  }, []);

  useEffect(loadSetup, [loadSetup]);

  useEffect(() => {
    const timer = window.setInterval(() => setClock(new Date()), MINUTE_MS);
    return () => window.clearInterval(timer);
  }, []);

  const load = useCallback(() => {
    if (channel === null || date === "") return () => undefined;
    let current = true;
    setLoading(true);
    setFailure(null);
    getPreview(channel, date)
      .then((result) => {
        if (current) setLineup(result);
      })
      .catch((err: unknown) => {
        if (!current) return;
        setLineup(null);
        setFailure(toFailure(err));
      })
      .finally(() => {
        if (current) setLoading(false);
      });
    return () => {
      current = false;
    };
  }, [channel, date]);

  useEffect(load, [load]);

  const channels = useMemo(() => (config ? sortChannels(config.channel) : []), [config]);
  const selected = channels.find((c) => c.number === channel) ?? null;
  const channelFolder = selected ? effectiveFolder(selected) : "";
  const rows = useMemo(() => (lineup ? groupAirings(lineup.airings) : []), [lineup]);
  const isToday = date === localDate(clock);
  const position = isToday && lineup !== null ? nowPosition(rows, localMs(clock)) : null;

  function showCell(row: GuideRow) {
    if (row.kind !== "episode") return null;
    const show = library ? findShow(library.show, channelFolder, row.airing.show) : null;
    const name = show?.name ?? row.airing.show ?? "";
    return (
      <>
        {show !== null && show.icon !== "" ? <IconImage src={mediaUrl(show.icon)} /> : <span className="icon" />}
        <span className="guide-show">{name}</span>
      </>
    );
  }

  const nowRow = (
    <li key="now" className="guide-now">
      <span className="guide-time">{formatClock(localMs(clock))}</span>
      <span className="guide-now-label">Now</span>
    </li>
  );

  const items: JSX.Element[] = [];
  rows.forEach((row, i) => {
    if (position !== null && position.index === i) items.push(nowRow);
    const classes = ["guide-row", row.kind, position !== null && position.index === i && position.current ? "current" : ""];
    items.push(
      <li key={row.key} className={classes.filter((c) => c !== "").join(" ")}>
        <span className="guide-time">
          {formatClock(row.startMs)}–{formatClock(row.endMs)}
        </span>
        <span className={`kind-badge ${row.kind}`}>{row.kind === "upnext" ? "up next" : row.kind}</span>
        {row.kind === "ad" ? (
          <span className="grow">{adBreakLabel(row, formatDuration(row.totalMs))}</span>
        ) : row.kind === "upnext" ? (
          <span className="grow">Up next</span>
        ) : (
          <>
            {showCell(row)}
            <span className="guide-code">{formatEpisodeCode(row.airing.season, row.airing.episode)}</span>
            <span className="grow">{row.airing.title ?? ""}</span>
          </>
        )}
      </li>
    );
  });
  if (position !== null && position.index >= rows.length) items.push(nowRow);

  return (
    <section>
      <div className="row">
        <h2 className="grow">Preview</h2>
        <select value={channel ?? ""} disabled={channels.length === 0} onChange={(e) => setChannel(Number(e.target.value))}>
          {channels.map((c) => (
            <option key={c.number} value={c.number}>
              {c.number} {c.name}
            </option>
          ))}
        </select>
        <input type="date" value={date} onChange={(e) => setDate(e.target.value)} />
        <button onClick={() => setDate(localDate(new Date()))}>Today</button>
        <button onClick={load} disabled={loading || channel === null}>
          Refresh
        </button>
      </div>
      {loadError !== null && (
        <div className="banner error row">
          <span className="grow">{loadError}</span>
          <button onClick={loadSetup}>Retry</button>
        </div>
      )}
      {config !== null && channels.length === 0 && <p className="muted">No channels yet. Add one on the Channels tab.</p>}
      <FailureBanner failure={failure} onReload={load} />
      {lineup !== null && lineup.warnings.length > 0 && (
        <div className="banner warn">
          <ul>
            {lineup.warnings.map((issue, i) => (
              <li key={i}>{`${issue.slot >= 0 ? `Slot ${issue.slot + 1}${issue.item >= 0 ? `, item ${issue.item + 1}` : ""}: ` : issue.item >= 0 ? `Loop item ${issue.item + 1}: ` : ""}${issue.message}`}</li>
            ))}
          </ul>
        </div>
      )}
      {loading && lineup === null && <p className="muted">Loading…</p>}
      {lineup !== null && (
        <div className={loading ? "card guide busy" : "card guide"}>
          {rows.length === 0 ? <p className="muted">Nothing airs on this date.</p> : <ul className="guide-list">{items}</ul>}
          <p className="muted guide-footer">
            Channel {lineup.channel} · {lineup.date} · hash <code>{lineup.hash}</code>
          </p>
        </div>
      )}
    </section>
  );
}
