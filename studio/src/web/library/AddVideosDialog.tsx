import { Fragment, useEffect, useRef, useState } from "react";
import { type ImportEntry, type ScanResult, type UploadResult, getImports, scanImports, uploadVideos } from "../api";
import { formatBytes } from "../format";
import { crumbs, joinRel, rejectedLine, tickedFiles } from "../uploading";
import { Dialog } from "./Dialog";

export interface Picked {
  dir: string;
  files: ScanResult[];
  uploaded?: boolean;
}

interface Props {
  onPicked: (picked: Picked) => void;
  onClose: () => void;
}

interface TabProps {
  busy: boolean;
  onBusy: (busy: boolean) => void;
  onPicked: (picked: Picked) => void;
}

const TABS = ["Upload", "Imports folder"] as const;
type Tab = (typeof TABS)[number];

export function AddVideosDialog({ onPicked, onClose }: Props) {
  const [tab, setTab] = useState<Tab>("Upload");
  const [busy, setBusy] = useState(false);
  const open = useRef(true);

  useEffect(() => {
    open.current = true;
    return () => {
      open.current = false;
    };
  }, []);

  const pass = (picked: Picked) => {
    if (open.current) onPicked(picked);
  };

  return (
    <Dialog title="Add videos" onClose={onClose}>
      <div className="row">
        {TABS.map((name) => (
          <button key={name} className={name === tab ? "pill on" : "pill"} disabled={busy} onClick={() => setTab(name)}>
            {name}
          </button>
        ))}
      </div>
      {tab === "Upload" ? <UploadTab busy={busy} onBusy={setBusy} onPicked={pass} /> : <ImportsTab busy={busy} onBusy={setBusy} onPicked={pass} />}
    </Dialog>
  );
}

function UploadTab({ busy, onBusy, onPicked }: TabProps) {
  const [files, setFiles] = useState<File[]>([]);
  const [ticked, setTicked] = useState<boolean[]>([]);
  const [ratio, setRatio] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<UploadResult | null>(null);
  const controller = useRef<AbortController>();

  useEffect(() => () => controller.current?.abort(), []);

  const chosen = tickedFiles(files, ticked);

  async function submit() {
    const abort = new AbortController();
    controller.current = abort;
    onBusy(true);
    setError(null);
    setResult(null);
    setRatio(0);
    try {
      const uploaded = await uploadVideos(chosen, setRatio, abort.signal);
      if (abort.signal.aborted) return;
      if (uploaded.files.length > 0 && uploaded.rejected.length === 0) onPicked({ dir: "", files: uploaded.files, uploaded: true });
      else setResult(uploaded);
    } catch (err) {
      if (!abort.signal.aborted) setError((err as Error).message);
    } finally {
      onBusy(false);
    }
  }

  const total = chosen.reduce((sum, file) => sum + file.size, 0);
  const keptAny = result !== null && result.files.length > 0;
  return (
    <div>
      <div className="form-grid">
        <label>
          Videos
          <input
            type="file"
            multiple
            accept="video/*"
            disabled={busy}
            onChange={(e) => {
              const picked = Array.from(e.target.files ?? []);
              setFiles(picked);
              setTicked(picked.map(() => true));
              setResult(null);
              setError(null);
            }}
          />
        </label>
      </div>
      {files.length > 0 && (
        <>
          <ul className="plain">
            {files.map((file, i) => (
              <li key={i}>
                <label>
                  <input
                    type="checkbox"
                    checked={ticked[i] === true}
                    disabled={busy || result !== null}
                    onChange={(e) => setTicked((prev) => prev.map((on, k) => (k === i ? e.target.checked : on)))}
                  />{" "}
                  {file.name} <span className="muted">{formatBytes(file.size)}</span>
                </label>
              </li>
            ))}
          </ul>
          <p className="muted">
            {chosen.length} of {files.length} {files.length === 1 ? "file" : "files"} · {formatBytes(total)}
          </p>
        </>
      )}
      {busy && (
        <div className="job">
          <div className="job-head">
            <span>Uploading…</span>
            <span>{Math.round(ratio * 100)}%</span>
          </div>
          <progress max={1} value={ratio} />
        </div>
      )}
      {error !== null && <div className="banner error">{error}</div>}
      {result !== null && (
        <div className={keptAny ? "banner warn" : "banner error"}>
          <strong>{keptAny ? "Some videos could not be added." : "None of these videos could be added."}</strong>
          <ul>
            {result.rejected.map((r, i) => (
              <li key={i}>{rejectedLine(r)}</li>
            ))}
          </ul>
        </div>
      )}
      <div className="row end">
        {keptAny && (
          <button className="primary" onClick={() => onPicked({ dir: "", files: result.files, uploaded: true })}>
            Continue with {result.files.length} {result.files.length === 1 ? "video" : "videos"}
          </button>
        )}
        <button className={result === null ? "primary" : undefined} disabled={busy || chosen.length === 0 || result !== null} onClick={submit}>
          Upload
        </button>
      </div>
    </div>
  );
}

function ImportsTab({ busy, onBusy, onPicked }: TabProps) {
  const [dir, setDir] = useState("");
  const [entries, setEntries] = useState<ImportEntry[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let current = true;
    setEntries(null);
    setError(null);
    getImports(dir)
      .then((listing) => {
        if (current) setEntries(listing.entries);
      })
      .catch((err: Error) => {
        if (current) setError(err.message);
      });
    return () => {
      current = false;
    };
  }, [dir]);

  async function use() {
    onBusy(true);
    setError(null);
    try {
      const scanned = await scanImports(dir);
      if (scanned.files.length === 0) setError("There are no videos in this folder.");
      else onPicked(scanned);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      onBusy(false);
    }
  }

  return (
    <div>
      <div className="row">
        {crumbs(dir).map((crumb, i, all) => (
          <Fragment key={crumb.rel}>
            {i > 0 && <span className="muted">/</span>}
            <button disabled={busy || i === all.length - 1} onClick={() => setDir(crumb.rel)}>
              {crumb.name}
            </button>
          </Fragment>
        ))}
      </div>
      {error !== null && <div className="banner error">{error}</div>}
      {entries === null && error === null && <p className="muted">Loading…</p>}
      {entries !== null && entries.length === 0 && <p className="muted">This folder is empty.</p>}
      {entries !== null && entries.length > 0 && (
        <ul className="plain picker">
          {entries.map((entry) => (
            <li key={`${entry.kind}:${entry.name}`}>
              {entry.kind === "folder" ? (
                <button className="entry" disabled={busy} onClick={() => setDir(joinRel(dir, entry.name))}>
                  {entry.name}/
                </button>
              ) : (
                <span className="entry muted">{entry.name}</span>
              )}
            </li>
          ))}
        </ul>
      )}
      <div className="row end">
        {busy && <span className="muted grow">Reading videos…</span>}
        <button className="primary" disabled={busy || entries === null} onClick={use}>
          Use this folder
        </button>
      </div>
    </div>
  );
}
