import { Fragment, useEffect, useState } from "react";
import { type ImportEntry, type ScanResult, getImports, scanImports } from "../api";
import { PillButton } from "../ui/controls";
import { Banner, EmptyState, Panel } from "../ui/layout";
import { crumbs, joinRel } from "../uploading";

interface Props {
  onScanned: (scanned: { dir: string; files: ScanResult[] }) => void;
}

export function ImportsBrowser({ onScanned }: Props) {
  const [dir, setDir] = useState("");
  const [entries, setEntries] = useState<ImportEntry[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

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
    setBusy(true);
    setError(null);
    try {
      const scanned = await scanImports(dir);
      if (scanned.files.length === 0) setError("There are no videos in this folder.");
      else onScanned(scanned);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <Panel
      title="Imports folder"
      actions={
        <PillButton kind="primary" disabled={busy || entries === null} onClick={use}>
          {busy ? "Reading videos…" : "Use this folder"}
        </PillButton>
      }
    >
      <div className="ui-line ui-crumbs">
        {crumbs(dir).map((crumb, i, all) => (
          <Fragment key={crumb.rel}>
            {i > 0 && <span className="ui-muted">/</span>}
            <button className="ui-crumb" disabled={busy || i === all.length - 1} onClick={() => setDir(crumb.rel)}>
              {crumb.name}
            </button>
          </Fragment>
        ))}
      </div>
      {error !== null && <Banner kind="error">{error}</Banner>}
      {entries === null && error === null && <EmptyState>Loading…</EmptyState>}
      {entries !== null && entries.length === 0 && <EmptyState>This folder is empty.</EmptyState>}
      {entries !== null && entries.length > 0 && (
        <ul className="ui-entries">
          {entries.map((entry) =>
            entry.kind === "folder" ? (
              <li key={`folder:${entry.name}`}>
                <button className="ui-entry folder" disabled={busy} onClick={() => setDir(joinRel(dir, entry.name))}>
                  {entry.name}/
                </button>
              </li>
            ) : (
              <li key={`file:${entry.name}`} className="ui-entry">
                {entry.name}
              </li>
            ),
          )}
        </ul>
      )}
    </Panel>
  );
}
