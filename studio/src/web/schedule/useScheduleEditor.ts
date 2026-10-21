import { useCallback, useEffect, useMemo, useState } from "react";
import { type Config, type Issue, getConfig, getLibrary, putConfig, reshuffle } from "../api";
import { sortChannels } from "../channels";
import { type Failure, toFailure } from "../failure";
import type { WebLibrary } from "../tree";
import { type Schedule, applySchedule, sameSchedule, scheduleOf } from "./editing";
import { type History, canRedo, canUndo, createHistory, pushHistory, redo, undo } from "./history";

const EMPTY_LIBRARY: WebLibrary = { version: 1, generated: "", ad: [], show: [] };

export interface SaveFailure extends Failure {
  schedule: Schedule;
}

export function useScheduleEditor() {
  const [config, setConfig] = useState<Config | null>(null);
  const [mtimeMs, setMtimeMs] = useState(0);
  const [library, setLibrary] = useState<WebLibrary>(EMPTY_LIBRARY);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [libraryError, setLibraryError] = useState<string | null>(null);
  const [selected, setSelected] = useState<number | null>(null);
  const [histories, setHistories] = useState<Record<number, History<Schedule>>>({});
  const [failure, setFailure] = useState<SaveFailure | null>(null);
  const [warnings, setWarnings] = useState<Issue[]>([]);
  const [busy, setBusy] = useState(false);

  const load = useCallback(() => {
    setLoadError(null);
    setFailure(null);
    setWarnings([]);
    setHistories({});
    getConfig()
      .then((result) => {
        setConfig(result.config);
        setMtimeMs(result.mtimeMs);
      })
      .catch((err: Error) => setLoadError(err.message));
    setLibraryError(null);
    getLibrary()
      .then(setLibrary)
      .catch((err: Error) => {
        setLibrary(EMPTY_LIBRARY);
        setLibraryError(err.message);
      });
  }, []);

  useEffect(load, [load]);

  const channels = useMemo(() => (config ? sortChannels(config.channel) : []), [config]);
  const channel = channels.find((c) => c.number === selected) ?? channels[0] ?? null;
  const saved = useMemo(() => (channel ? scheduleOf(channel) : null), [channel]);
  const history = channel ? histories[channel.number] ?? (saved ? createHistory(saved) : null) : null;
  const schedule = history?.present ?? null;
  const dirty = schedule !== null && saved !== null && !sameSchedule(schedule, saved);

  const setHistory = useCallback(
    (update: (h: History<Schedule>) => History<Schedule>) => {
      if (!channel || !history) return;
      setHistories((all) => ({ ...all, [channel.number]: update(all[channel.number] ?? history) }));
    },
    [channel, history]
  );

  const edit = useCallback((next: Schedule) => setHistory((h) => pushHistory(h, next)), [setHistory]);

  function selectChannel(number: number) {
    if (channel && saved && dirty) {
      const current = channel.number;
      setHistories((all) => ({ ...all, [current]: createHistory(saved) }));
    }
    setFailure(null);
    setWarnings([]);
    setSelected(number);
  }

  async function save() {
    if (!config || !channel || !schedule) return;
    const next = applySchedule(config, channel.number, schedule);
    setBusy(true);
    setFailure(null);
    try {
      const result = await putConfig(next, mtimeMs);
      setConfig(next);
      setMtimeMs(result.mtimeMs);
      setWarnings(result.issues.filter((i) => i.level === "warning"));
    } catch (err) {
      setFailure({ ...toFailure(err), schedule });
    } finally {
      setBusy(false);
    }
  }

  async function shuffle() {
    if (!config || !schedule || dirty) return;
    setBusy(true);
    setFailure(null);
    try {
      const result = await reshuffle(mtimeMs);
      setConfig({ ...config, seed: result.seed });
      setMtimeMs(result.mtimeMs);
    } catch (err) {
      setFailure({ ...toFailure(err), schedule });
    } finally {
      setBusy(false);
    }
  }

  return {
    config,
    library,
    loadError,
    libraryError,
    channels,
    channel,
    schedule,
    dirty,
    busy,
    failure,
    warnings,
    load,
    edit,
    selectChannel,
    save,
    shuffle,
    canUndo: history !== null && canUndo(history),
    canRedo: history !== null && canRedo(history),
    undo: () => setHistory(undo),
    redo: () => setHistory(redo)
  };
}
