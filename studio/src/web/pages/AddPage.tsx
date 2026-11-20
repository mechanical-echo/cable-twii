import { useEffect, useRef, useState } from "react";
import { type AddGroup, type AddRow, commonShow, groupRows, markExisting, rowsFromFiles, rowsFromScan, summary } from "../adding";
import { type Config, type Job, type ScanResult, checkUploads, getConfig, getJobs, getLibrary, pickSource, putConfig, sourceUrl } from "../api";
import { NEW_CHANNEL, applyEdit, channelChoices, effectiveFolder, newDraft, validateDraft } from "../channels";
import { baseName, formatBytes } from "../format";
import { AddGroupPanel, type GroupEdit } from "../library/AddGroupPanel";
import { ImportsBrowser } from "../library/ImportsBrowser";
import { SourcePreview } from "../library/SourcePreview";
import type { Route } from "../router";
import type { WebLibrary } from "../tree";
import { PillButton } from "../ui/controls";
import { Banner, EmptyState, PageHead, Panel } from "../ui/layout";
import { type AddPlan, type AddStart, type Uploads, clearHandOff, readHandOff } from "../useUploads";

interface Props {
  route: Extract<Route, { page: "add" }>;
  server: boolean;
  uploads: Uploads;
  onDone: () => void;
}

interface Base {
  library: WebLibrary;
  config: Config;
  mtimeMs: number;
  jobs: Job[];
}

type Picked = { kind: "files"; count: number } | { kind: "scan"; dir: string; count: number };

const NEW_CHANNEL_CATEGORY = "General";
const DURATION_TIMEOUT_MS = 10000;

function readDuration(file: File): Promise<number | null> {
  return new Promise((resolve) => {
    const video = document.createElement("video");
    const url = URL.createObjectURL(file);
    let settled = false;
    const done = (ms: number | null) => {
      if (settled) return;
      settled = true;
      URL.revokeObjectURL(url);
      video.removeAttribute("src");
      resolve(ms);
    };
    video.preload = "metadata";
    video.onloadedmetadata = () => done(Number.isFinite(video.duration) ? Math.round(video.duration * 1000) : null);
    video.onerror = () => done(null);
    setTimeout(() => done(null), DURATION_TIMEOUT_MS);
    video.src = url;
  });
}

function rearm(rows: AddRow[]): AddRow[] {
  return rows.map((r) => ((r.note === "on-air" || r.note === "in-batch") && !r.ticked ? { ...r, ticked: true } : r));
}

function plural(n: number, one: string, many: string): string {
  return `${n} ${n === 1 ? one : many}`;
}

export function AddPage({ route, server, uploads, onDone }: Props) {
  const ads = route.to === "ads";
  const [start] = useState<AddStart | null>(() => readHandOff());
  const [base, setBase] = useState<Base | null>(null);
  const [step, setStep] = useState<"choose" | "imports" | "rows">(start !== null && "from" in start && start.from === "imports" ? "imports" : "choose");
  const [picked, setPicked] = useState<Picked | null>(null);
  const [edits, setEdits] = useState<GroupEdit[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [preview, setPreview] = useState<{ src: string; title: string; objectUrl: boolean } | null>(null);
  const [over, setOver] = useState(false);
  const fileInput = useRef<HTMLInputElement>(null);
  const alive = useRef(true);

  useEffect(() => {
    alive.current = true;
    clearHandOff();
    return () => {
      alive.current = false;
    };
  }, []);

  useEffect(() => {
    Promise.all([getLibrary(), getConfig(), getJobs()])
      .then(([library, { config, mtimeMs }, jobs]) => {
        if (alive.current) setBase({ library, config, mtimeMs, jobs });
      })
      .catch((err: Error) => setError(err.message));
  }, []);

  const preselected = (config: Config): string | null => {
    const channel = route.channel === null ? undefined : config.channel.find((c) => c.number === route.channel);
    return channel === undefined ? null : effectiveFolder(channel);
  };

  function mark(group: AddGroup, b: Base): AddGroup {
    return ads ? group : markExisting({ ...group, rows: rearm(group.rows) }, b.library, b.jobs);
  }

  function show(rows: AddRow[], fallback: string, b: Base) {
    const groups: AddGroup[] = ads
      ? [{ show: "", rows, dest: { channel: preselected(b.config) ?? "", show: "", isNewShow: false } }]
      : groupRows(rows, fallback, b.library, b.config, route.channel).map((g) => markExisting(g, b.library, b.jobs));
    setEdits(groups.map((group) => ({ group, newChannel: "", icon: null })));
    setStep("rows");
    void loadDurations(rows);
  }

  async function loadDurations(rows: AddRow[]) {
    for (const row of rows) {
      if (row.file === null || row.durationMs !== null) continue;
      const durationMs = await readDuration(row.file);
      if (!alive.current) return;
      if (durationMs !== null) patchRow(row.id, { durationMs }, false);
    }
  }

  async function takeFiles(files: File[], b: Base) {
    if (files.length === 0) return;
    setBusy(true);
    setError(null);
    try {
      const added = await checkUploads(files.map((f) => ({ name: f.name, size: f.size })));
      if (!alive.current) return;
      const rows = rowsFromFiles(files, added);
      setPicked({ kind: "files", count: files.length });
      show(rows, commonShow(rows), b);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  function takeScan(scanned: { dir: string; files: ScanResult[] }, b: Base) {
    if (scanned.files.length === 0) {
      setError("There are no videos in this folder.");
      return;
    }
    setPicked({ kind: "scan", dir: scanned.dir, count: scanned.files.length });
    show(rowsFromScan(scanned.files), baseName(scanned.dir), b);
  }

  async function pickFolder(b: Base) {
    setBusy(true);
    setError(null);
    try {
      const scanned = await pickSource();
      if (scanned !== null && alive.current) takeScan(scanned, b);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  const started = useRef(false);
  useEffect(() => {
    if (base === null || start === null || started.current) return;
    started.current = true;
    if ("files" in start) void takeFiles(start.files, base);
    else if (start.from === "pick") void pickFolder(base);
  }, [base]);

  function patchRow(id: string, change: Partial<AddRow>, remark: boolean) {
    setEdits((prev) =>
      prev.map((edit) => {
        if (!edit.group.rows.some((r) => r.id === id)) return edit;
        const rows = edit.group.rows.map((r) => (r.id === id ? { ...r, ...change } : r));
        const group = { ...edit.group, rows };
        return { ...edit, group: remark && base !== null ? mark(group, base) : group };
      }),
    );
  }

  function onRow(id: string, change: Partial<AddRow>) {
    if (base === null) return;
    const row = edits.flatMap((e) => e.group.rows).find((r) => r.id === id);
    if (row === undefined) return;
    if (change.ticked === true && row.replaces !== null) {
      const old = base.library.show.flatMap((s) => s.episode).find((e) => e.path === row.replaces);
      if (old !== undefined) change = { ...change, title: old.title };
    }
    patchRow(id, change, "season" in change || "episode" in change);
  }

  function patchGroup(index: number, change: (edit: GroupEdit, b: Base) => GroupEdit) {
    if (base === null) return;
    setEdits((prev) => prev.map((edit, i) => (i === index ? change(edit, base) : edit)));
  }

  function onChannel(index: number, channel: string) {
    patchGroup(index, (edit, b) => {
      const { group } = edit;
      const wanted = [group.dest.show.toLowerCase(), group.show.toLowerCase()];
      const match = b.library.show.find((s) => s.channel === channel && (wanted.includes(s.folder.toLowerCase()) || wanted.includes(s.name.toLowerCase())));
      const dest = match !== undefined ? { channel, show: match.folder, isNewShow: false } : { channel, show: group.dest.isNewShow ? group.dest.show : group.show, isNewShow: true };
      return { ...edit, group: mark({ ...group, dest }, b) };
    });
  }

  function onShow(index: number, showName: string, isNew: boolean) {
    patchGroup(index, (edit, b) => ({ ...edit, group: mark({ ...edit.group, dest: { ...edit.group.dest, show: showName, isNewShow: isNew } }, b) }));
  }

  function openPreview(row: AddRow) {
    if (row.file !== null) setPreview({ src: URL.createObjectURL(row.file), title: row.name, objectUrl: true });
    else if (row.source !== null) setPreview({ src: sourceUrl(row.source), title: row.name, objectUrl: false });
  }

  function closePreview() {
    if (preview?.objectUrl) URL.revokeObjectURL(preview.src);
    setPreview(null);
  }

  const allRows = edits.flatMap((e) => e.group.rows);
  const totals = summary(allRows);
  const live = edits.filter((e) => e.group.rows.some((r) => r.ticked));
  const ready =
    base !== null &&
    !busy &&
    totals.ticked > 0 &&
    live.every((e) => {
      const { dest } = e.group;
      if (dest.channel === NEW_CHANNEL) return e.newChannel.trim() !== "";
      return ads || (dest.channel !== "" && dest.show.trim() !== "");
    });

  async function submit() {
    if (base === null || !ready) return;
    setBusy(true);
    setError(null);
    try {
      let config = base.config;
      const made = new Map<string, string>();
      for (const edit of live) {
        if (edit.group.dest.channel !== NEW_CHANNEL) continue;
        const name = edit.newChannel.trim();
        if (made.has(name.toLowerCase())) continue;
        const draft = { ...newDraft(config.channel), name, category: NEW_CHANNEL_CATEGORY };
        const problems = validateDraft(draft, config.channel, null);
        if (problems.length > 0) throw new Error(problems[0]);
        config = applyEdit(config, null, draft);
        made.set(name.toLowerCase(), name);
      }
      if (config !== base.config) await putConfig(config, base.mtimeMs);
      const plans: AddPlan[] = live.map((edit) => {
        const { dest, rows } = edit.group;
        const channelFolder = dest.channel === NEW_CHANNEL ? made.get(edit.newChannel.trim().toLowerCase())! : dest.channel;
        if (ads) return { kind: "ads", channelFolder, rows };
        const showName = dest.isNewShow ? dest.show.trim() : "";
        return { kind: "show", channelFolder, showFolder: dest.show.trim(), showName, icon: dest.isNewShow ? edit.icon : null, rows };
      });
      uploads.start(plans);
      onDone();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  const libraryChannels = base === null ? [] : [...base.library.show.map((s) => s.channel), ...base.library.ad.map((a) => a.channel)];
  const channelOptions = base === null
    ? []
    : [
        ...(ads ? [{ value: "", label: "All channels" }] : []),
        ...channelChoices(base.config, libraryChannels).map((folder) => ({
          value: folder,
          label: base.config.channel.find((c) => effectiveFolder(c) === folder)?.name ?? folder,
        })),
        { value: NEW_CHANNEL, label: "New channel…" },
      ];
  const showOptions = (channel: string) =>
    base === null ? [] : base.library.show.filter((s) => s.channel === channel).map((s) => ({ value: s.folder, label: s.name }));

  const noun = ads ? ["ad", "ads"] : ["episode", "episodes"];
  const title = ads ? "Add ads" : "Add episodes";
  let sub: string | undefined;
  if (picked?.kind === "files") sub = `${plural(picked.count, "file", "files")} picked from this computer`;
  if (picked?.kind === "scan") sub = `${plural(picked.count, "video", "videos")} in ${server ? baseName(picked.dir) : picked.dir}`;

  return (
    <section>
      <PageHead
        title={title}
        sub={sub}
        actions={
          step === "rows" ? (
            <PillButton
              onClick={() => {
                setEdits([]);
                setPicked(null);
                setStep("choose");
              }}
            >
              Choose other files
            </PillButton>
          ) : undefined
        }
      />
      {error !== null && <Banner kind="error">{error}</Banner>}
      {base === null && error === null && <EmptyState>Loading…</EmptyState>}
      {base !== null && step === "choose" && (
        <Panel>
          {server ? (
            <div
              className={over ? "ui-drop over" : "ui-drop"}
              role="button"
              tabIndex={0}
              onClick={() => fileInput.current?.click()}
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.preventDefault();
                  fileInput.current?.click();
                }
              }}
              onDragOver={(e) => {
                e.preventDefault();
                setOver(true);
              }}
              onDragLeave={() => setOver(false)}
              onDrop={(e) => {
                e.preventDefault();
                setOver(false);
                void takeFiles(Array.from(e.dataTransfer.files), base);
              }}
            >
              <b>{busy ? "Checking files…" : "Drop video files here"}</b>
              <small>or click to pick them from this computer</small>
              <input
                ref={fileInput}
                type="file"
                multiple
                accept="video/*"
                hidden
                onChange={(e) => {
                  const files = Array.from(e.target.files ?? []);
                  e.target.value = "";
                  void takeFiles(files, base);
                }}
              />
            </div>
          ) : null}
          <div className="ui-line">
            {server ? (
              <PillButton disabled={busy} onClick={() => setStep("imports")}>
                Use the imports folder
              </PillButton>
            ) : (
              <PillButton kind="primary" disabled={busy} onClick={() => void pickFolder(base)}>
                {busy ? "Reading videos…" : "Pick a folder"}
              </PillButton>
            )}
            <span className="grow" />
            <PillButton onClick={onDone}>Cancel</PillButton>
          </div>
        </Panel>
      )}
      {base !== null && step === "imports" && (
        <>
          <ImportsBrowser onScanned={(scanned) => takeScan(scanned, base)} />
          <div className="ui-line end ui-gap">
            <PillButton onClick={() => setStep("choose")}>Back</PillButton>
          </div>
        </>
      )}
      {base !== null && step === "rows" && (
        <>
          {edits.map((edit, i) => (
            <AddGroupPanel
              key={i}
              edit={edit}
              ads={ads}
              channels={channelOptions}
              shows={showOptions(edit.group.dest.channel)}
              onChannel={(channel) => onChannel(i, channel)}
              onShow={(name, isNew) => onShow(i, name, isNew)}
              onNewChannel={(name) => patchGroup(i, (e) => ({ ...e, newChannel: name }))}
              onIcon={(icon) => patchGroup(i, (e) => ({ ...e, icon }))}
              onRow={onRow}
              onPreview={openPreview}
            />
          ))}
          <div className="ui-foot">
            <PillButton onClick={onDone}>Cancel</PillButton>
            <span className="ui-muted grow">
              {totals.ticked} of {totals.total} ticked{picked?.kind === "files" ? ` · ${formatBytes(totals.bytes)} to upload` : ""}
            </span>
            <PillButton kind="primary" disabled={!ready} onClick={() => void submit()}>
              Add {plural(totals.ticked, noun[0]!, noun[1]!)}
            </PillButton>
          </div>
        </>
      )}
      {preview !== null && <SourcePreview src={preview.src} title={preview.title} onClose={closePreview} />}
    </section>
  );
}
