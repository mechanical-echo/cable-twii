import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { type ScanResult, type WebBuildResult, getLibrary, mediaUrl, pickSource, rebuildLibrary } from "../api";
import { baseName, formatDuration } from "../format";
import { ImportAdsDialog } from "../library/ImportAdsDialog";
import { ImportShowDialog } from "../library/ImportShowDialog";
import { JobsPanel } from "../library/JobsPanel";
import { ShowMetaDialog } from "../library/ShowMetaDialog";
import { type ShowNode, type WebLibrary, buildTree, seasonLabel } from "../tree";

type Picked = { dir: string; files: ScanResult[] };
type Dialog = { kind: "show"; picked: Picked } | { kind: "ads"; picked: Picked } | { kind: "meta"; node: ShowNode } | null;

const REFRESH_DELAY_MS = 2600;

export function LibraryPage() {
  const [library, setLibrary] = useState<WebLibrary | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [dialog, setDialog] = useState<Dialog>(null);
  const [busy, setBusy] = useState(false);
  const [report, setReport] = useState<Pick<WebBuildResult, "skipped" | "warnings"> | null>(null);
  const timer = useRef<ReturnType<typeof setTimeout>>();

  const load = useCallback(() => {
    getLibrary()
      .then(setLibrary)
      .catch((err: Error) => setError(err.message));
  }, []);

  const refreshSoon = useCallback(() => {
    clearTimeout(timer.current);
    timer.current = setTimeout(load, REFRESH_DELAY_MS);
  }, [load]);

  useEffect(() => {
    load();
    return () => clearTimeout(timer.current);
  }, [load]);

  const tree = useMemo(() => (library ? buildTree(library) : []), [library]);
  const channels = tree.map((c) => c.channel).filter((c) => c !== "");

  async function pick(kind: "show" | "ads") {
    setError(null);
    try {
      const picked = await pickSource();
      if (picked) setDialog({ kind, picked });
    } catch (err) {
      setError((err as Error).message);
    }
  }

  async function rebuild() {
    setBusy(true);
    setError(null);
    try {
      const result = await rebuildLibrary();
      setLibrary(result.library);
      setReport({ skipped: result.skipped, warnings: result.warnings });
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <section>
      <div className="row">
        <h2 className="grow">Library</h2>
        <button className="primary" onClick={() => pick("show")}>
          Add show…
        </button>
        <button onClick={() => pick("ads")}>Add ads…</button>
        <button disabled={busy} onClick={rebuild}>
          Rebuild
        </button>
      </div>
      {error !== null && <div className="banner error">{error}</div>}
      {report !== null && (report.skipped.length > 0 || report.warnings.length > 0) && (
        <div className="banner warn">
          <strong>Rebuilt with notes</strong>
          <ul>
            {report.skipped.map((s) => (
              <li key={s.path}>
                Skipped {s.path}: {s.reason}
              </li>
            ))}
            {report.warnings.map((w) => (
              <li key={w}>{w}</li>
            ))}
          </ul>
        </div>
      )}
      <JobsPanel onFinished={refreshSoon} />
      {library !== null && tree.length === 0 && <p className="muted">The library is empty. Add a show to get started.</p>}
      {tree.map((ch) => (
        <div key={ch.channel} className="card">
          <h3>{ch.channel === "" ? "All channels" : ch.channel}</h3>
          {ch.shows.map((node) => (
            <details key={node.show.folder} className="show" open>
              <summary>
                {node.show.icon !== "" && <img className="icon" src={mediaUrl(node.show.icon)} alt="" />}
                <span className="grow">{node.show.name}</span>
                <button
                  onClick={(e) => {
                    e.preventDefault();
                    setDialog({ kind: "meta", node });
                  }}
                >
                  Edit info
                </button>
              </summary>
              {node.seasons.map((s) => (
                <div key={s.season} className="season">
                  <h4>{seasonLabel(s.season)}</h4>
                  <ul className="plain">
                    {s.episodes.map((ep) => (
                      <li key={ep.path} className="ep">
                        <span>
                          {ep.season >= 1 ? `E${ep.episode} · ` : ""}
                          {ep.title}
                        </span>
                        <span className="muted">{formatDuration(ep.duration_ms)}</span>
                      </li>
                    ))}
                  </ul>
                </div>
              ))}
            </details>
          ))}
          {ch.ads.length > 0 && (
            <details className="show">
              <summary>Ads ({ch.ads.length})</summary>
              <ul className="plain">
                {ch.ads.map((ad) => (
                  <li key={ad.path} className="ep ad">
                    <span>{baseName(ad.path)}</span>
                    <span className="muted">{formatDuration(ad.duration_ms)}</span>
                  </li>
                ))}
              </ul>
            </details>
          )}
        </div>
      ))}
      {dialog?.kind === "show" && (
        <ImportShowDialog dir={dialog.picked.dir} files={dialog.picked.files} channels={channels} onClose={() => setDialog(null)} onImported={refreshSoon} />
      )}
      {dialog?.kind === "ads" && (
        <ImportAdsDialog dir={dialog.picked.dir} files={dialog.picked.files} channels={channels} onClose={() => setDialog(null)} onImported={refreshSoon} />
      )}
      {dialog?.kind === "meta" && <ShowMetaDialog node={dialog.node} onClose={() => setDialog(null)} onSaved={setLibrary} />}
    </section>
  );
}
