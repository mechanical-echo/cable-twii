import { useCallback, useEffect, useState } from "react";
import { type DriveInfo, getDrive } from "./api";
import { DrivePage } from "./pages/DrivePage";
import { LibraryPage } from "./pages/LibraryPage";

const TABS = ["Drive", "Library", "Channels", "Schedule", "Preview"] as const;
type Tab = (typeof TABS)[number];

export function App() {
  const [tab, setTab] = useState<Tab>("Drive");
  const [drive, setDrive] = useState<DriveInfo | null>(null);
  const [loaded, setLoaded] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);

  useEffect(() => {
    getDrive()
      .then((info) => {
        setDrive(info);
        if (info?.usable && info.hasRoot) setTab("Library");
      })
      .catch((err: Error) => setLoadError(err.message))
      .finally(() => setLoaded(true));
  }, []);

  const ready = drive !== null && drive.usable && drive.hasRoot;
  const onDrive = useCallback((info: DriveInfo | null) => setDrive(info), []);

  let page;
  if (!loaded) page = <p className="muted">Loading…</p>;
  else if (loadError !== null) page = <div className="banner error">{loadError}</div>;
  else if (tab === "Drive") page = <DrivePage drive={drive} onDrive={onDrive} />;
  else if (!ready) page = <div className="card">Choose and set up a drive on the Drive tab first.</div>;
  else if (tab === "Library") page = <LibraryPage />;
  else page = <div className="card">Coming soon.</div>;

  return (
    <div className="shell">
      <nav className="tabs">
        <h1>cable-twii Studio</h1>
        {TABS.map((name) => (
          <button key={name} className={name === tab ? "tab active" : "tab"} onClick={() => setTab(name)}>
            {name}
          </button>
        ))}
      </nav>
      <main className="content">{page}</main>
    </div>
  );
}
