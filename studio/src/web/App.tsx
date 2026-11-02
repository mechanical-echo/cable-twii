import { useCallback, useEffect, useState } from "react";
import { type DriveInfo, type ModeInfo, getDrive, getMode, onUnauthorized } from "./api";
import { LoginForm, LoginPage } from "./LoginPage";
import { ChannelsPage } from "./pages/ChannelsPage";
import { DrivePage } from "./pages/DrivePage";
import { LibraryPage } from "./pages/LibraryPage";
import { PreviewPage } from "./pages/PreviewPage";
import { SchedulePage } from "./pages/SchedulePage";
import { UNSAVED_PROMPT } from "./schedule/editing";
import { signInView } from "./signin";

const TABS = ["Drive", "Library", "Channels", "Schedule", "Preview"] as const;
type Tab = (typeof TABS)[number];

export function App() {
  const [tab, setTab] = useState<Tab>("Drive");
  const [mode, setMode] = useState<ModeInfo | null>(null);
  const [drive, setDrive] = useState<DriveInfo | null>(null);
  const [loaded, setLoaded] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);
  const [lost, setLost] = useState(false);
  const [session, setSession] = useState(0);

  const load = useCallback(() => {
    setLoadError(null);
    getMode()
      .then(async (next) => {
        setMode(next);
        if (!next.signedIn) return;
        const info = await getDrive();
        setDrive(info);
        if (info?.usable && info.hasRoot) setTab("Library");
      })
      .catch((err: Error) => setLoadError(err.message))
      .finally(() => setLoaded(true));
  }, []);

  useEffect(load, [load]);

  const signedOut = useCallback(() => {
    setDirty(false);
    setMode((current) => (current?.signedIn ? { ...current, signedIn: false } : current));
  }, []);

  useEffect(() => onUnauthorized(() => setLost(true)), []);

  const server = mode?.mode === "server";
  const view = signInView(mode, lost);
  const ready = server || (drive !== null && drive.usable && drive.hasRoot);
  const onDrive = useCallback((info: DriveInfo | null) => setDrive(info), []);

  function choose(name: Tab) {
    if (name === tab) return;
    if (dirty && !window.confirm(UNSAVED_PROMPT)) return;
    setTab(name);
  }

  if (mode !== null && view === "page") {
    return (
      <LoginPage
        name={mode.name}
        onSignedIn={() => {
          setLost(false);
          setMode({ ...mode, signedIn: true });
          setLoaded(false);
          load();
        }}
      />
    );
  }

  let page;
  if (!loaded) page = <p className="muted">Loading…</p>;
  else if (tab === "Drive") page = <DrivePage drive={drive} onDrive={onDrive} server={server ? { name: mode.name, onSignedOut: signedOut } : null} />;
  else if (!ready) page = <div className="card">Choose and set up a drive on the Drive tab first.</div>;
  else if (tab === "Library") page = <LibraryPage server={server} session={session} />;
  else if (tab === "Channels") page = <ChannelsPage />;
  else if (tab === "Schedule") page = <SchedulePage onDirty={setDirty} />;
  else page = <PreviewPage />;

  return (
    <div className={server ? "shell server" : "shell"}>
      <nav className="tabs">
        <h1>cable-twii Studio</h1>
        {TABS.map((name) => (
          <button key={name} className={name === tab ? "tab active" : "tab"} onClick={() => choose(name)}>
            {name === "Drive" && server ? "Server" : name}
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
      {view === "overlay" && (
        <div className="overlay">
          <LoginForm
            name={mode?.name ?? null}
            note="You were signed out. Sign in again to carry on where you left off."
            onSignedIn={() => {
              setLost(false);
              setSession((n) => n + 1);
            }}
          />
        </div>
      )}
    </div>
  );
}
