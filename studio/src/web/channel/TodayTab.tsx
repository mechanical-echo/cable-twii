import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { type LineupResult, getPreview, mediaUrl } from "../api";
import type { Channel } from "../../server/types.js";
import { effectiveFolder } from "../channels";
import { FailureBanner } from "../FailureBanner";
import { type Failure, toFailure } from "../failure";
import { formatDuration } from "../format";
import { IconImage } from "../IconImage";
import { createLatest } from "../latest";
import { type GuideRow, adBreakLabel, findShow, formatClock, formatEpisodeCode, groupAirings, localDate, localMs, nowPosition } from "../preview";
import type { WebLibrary } from "../tree";
import { PillButton, TextField } from "../ui/controls";
import { Banner, EmptyState, Panel } from "../ui/layout";
import { ProgressBar } from "../ui/status";

const MINUTE_MS = 60_000;

interface Props {
  channel: Channel;
  library: WebLibrary | null;
}

function issuePlace(slot: number, item: number): string {
  if (slot >= 0) return `Slot ${slot + 1}${item >= 0 ? `, item ${item + 1}` : ""}: `;
  return item >= 0 ? `Loop item ${item + 1}: ` : "";
}

export function TodayTab({ channel, library }: Props) {
  const [date, setDate] = useState(() => localDate(new Date()));
  const [lineup, setLineup] = useState<LineupResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [failure, setFailure] = useState<Failure | null>(null);
  const [clock, setClock] = useState(() => new Date());
  const latest = useRef(createLatest());
  const list = useRef<HTMLDivElement>(null);
  const scrolled = useRef<string | null>(null);

  useEffect(() => {
    const timer = window.setInterval(() => setClock(new Date()), MINUTE_MS);
    return () => window.clearInterval(timer);
  }, []);

  const load = useCallback(() => {
    const isLatest = latest.current.begin();
    setFailure(null);
    if (date === "") {
      setLineup(null);
      return;
    }
    setLoading(true);
    getPreview(channel.number, date)
      .then((result) => {
        if (isLatest()) setLineup(result);
      })
      .catch((err: unknown) => {
        if (!isLatest()) return;
        setLineup(null);
        setFailure(toFailure(err));
      })
      .finally(() => {
        if (isLatest()) setLoading(false);
      });
  }, [channel.number, date]);

  useEffect(() => {
    load();
    const pending = latest.current;
    return () => pending.cancel();
  }, [load]);

  const folder = effectiveFolder(channel);
  const rows = useMemo(() => (lineup ? groupAirings(lineup.airings) : []), [lineup]);
  const isToday = date === localDate(clock);
  const nowMs = localMs(clock);
  const position = isToday && lineup !== null ? nowPosition(rows, nowMs) : null;

  useEffect(() => {
    if (lineup === null || !isToday || scrolled.current === lineup.date) return;
    scrolled.current = lineup.date;
    list.current?.querySelector(".ui-air.on, .ui-air-now")?.scrollIntoView({ block: "center" });
  }, [lineup, isToday]);

  function label(row: GuideRow) {
    if (row.kind === "ad") return <span className="grow ui-air-ad">{adBreakLabel(row, formatDuration(row.totalMs))}</span>;
    if (row.kind === "upnext") return <span className="grow ui-muted">Up next</span>;
    const show = library ? findShow(library.show, folder, row.airing.show) : null;
    const code = formatEpisodeCode(row.airing.season, row.airing.episode);
    return (
      <>
        <span className="ui-air-icon">{show !== null && show.icon !== "" && <IconImage src={mediaUrl(show.icon)} />}</span>
        <span className="grow">
          <b>{[show?.name ?? row.airing.show ?? "", code].filter((p) => p !== "").join(" ")}</b>
          {row.airing.title ? ` · ${row.airing.title}` : ""}
        </span>
      </>
    );
  }

  const items: JSX.Element[] = [];
  rows.forEach((row, i) => {
    const here = position !== null && position.index === i;
    if (here && !position.current) {
      items.push(
        <div key="now" className="ui-air-now">
          <span className="ui-air-time">{formatClock(nowMs)}</span>
          <span>Now</span>
        </div>,
      );
    }
    const current = here && position.current;
    items.push(
      <div key={row.key} className={current ? "ui-air on" : "ui-air"}>
        <span className="ui-air-time">
          {formatClock(row.startMs)}–{formatClock(row.endMs)}
        </span>
        {label(row)}
        {current && <ProgressBar value={(nowMs - row.startMs) / (row.endMs - row.startMs)} />}
      </div>,
    );
  });

  return (
    <>
      <div className="ui-line ui-today-bar">
        <TextField type="date" value={date} onChange={setDate} label="Day" />
        <PillButton disabled={isToday} onClick={() => setDate(localDate(new Date()))}>
          Today
        </PillButton>
        {loading && <span className="ui-muted">Loading…</span>}
      </div>
      <FailureBanner failure={failure} onReload={load} />
      {lineup !== null && lineup.warnings.length > 0 && (
        <Banner kind="warn">
          <ul>
            {lineup.warnings.map((issue, i) => (
              <li key={i}>{`${issuePlace(issue.slot, issue.item)}${issue.message}`}</li>
            ))}
          </ul>
        </Banner>
      )}
      {lineup !== null && (
        <Panel>
          {rows.length === 0 ? <EmptyState>Nothing airs on this day.</EmptyState> : <div ref={list} className={loading ? "ui-airings busy" : "ui-airings"}>{items}</div>}
        </Panel>
      )}
    </>
  );
}
