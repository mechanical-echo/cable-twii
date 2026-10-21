import { useCallback, useEffect, useState } from "react";
import { type DriveInfo, getDrive } from "./api";
import { ChannelsPage } from "./pages/ChannelsPage";
import { DrivePage } from "./pages/DrivePage";
import { LibraryPage } from "./pages/LibraryPage";
import { PreviewPage } from "./pages/PreviewPage";
import { SchedulePage } from "./pages/SchedulePage";
import { UNSAVED_PROMPT } from "./schedule/editing";

const TABS = ["Drive", "Library", "Channels", "Schedule", "Preview"] as const;
type Tab = (typeof TABS)[number];

export function App() {
  const [tab, setTab] = useState<Tab>("Drive");
  const [drive, setDrive] = useState<DriveInfo | null>(null);
  const [loaded, setLoaded] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);

  const load = useCallback(() => {
    setLoadError(null);
    getDrive()
      .then((info) => {
        setDrive(info);
        if (info?.usable && info.hasRoot) setTab("Library");
      })
      .catch((err: Error) => setLoadError(err.message))
      .finally(() => setLoaded(true));
  }, []);

  useEffect(load, [load]);

  const ready = drive !== null && drive.usable && drive.hasRoot;
  const onDrive = useCallback((info: DriveInfo | null) => setDrive(info), []);

  function choose(name: Tab) {
    if (name === tab) return;
    if (dirty && !window.confirm(UNSAVED_PROMPT)) return;
    setTab(name);
  }

  let page;
  if (!loaded) page = <p className="muted">Loading…</p>;
  else if (tab === "Drive") page = <DrivePage drive={drive} onDrive={onDrive} />;
  else if (!ready) page = <div className="card">Choose and set up a drive on the Drive tab first.</div>;
  else if (tab === "Library") page = <LibraryPage />;
  else if (tab === "Channels") page = <ChannelsPage />;
  else if (tab === "Schedule") page = <SchedulePage onDirty={setDirty} />;
  else page = <PreviewPage />;

  return (
    <div className="shell">
      <nav className="tabs">
        <h1>cable-twii Studio</h1>
        {TABS.map((name) => (
          <button key={name} className={name === tab ? "tab active" : "tab"} onClick={() => choose(name)}>
            {name}
          </button>
        ))}
      </nav>
      <main className={tab === "Schedule" ? "content wide" : "content"}>
        {loadError !== null && (
          <div className="banner error row">
            <span className="grow">Could not read the drive setting: {loadError}</span>
            <button onClick={load}>Retry</button>
          </div>
        )}
        {page}
      </main>
    </div>
  );
}
