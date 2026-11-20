import { useCallback, useEffect, useState } from "react";
import { type DriveInfo, type ModeInfo, getDrive, getMode, onUnauthorized } from "./api";
import { LoginForm, LoginPage } from "./LoginPage";
import { AddPage } from "./pages/AddPage";
import { AdPage } from "./pages/AdPage";
import { AdsPage } from "./pages/AdsPage";
import { ChannelListPage } from "./pages/ChannelListPage";
import { ChannelPage } from "./pages/ChannelPage";
import { DrivePage } from "./pages/DrivePage";
import { EpisodePage } from "./pages/EpisodePage";
import { EpisodesPage } from "./pages/EpisodesPage";
import { FolderPage } from "./pages/FolderPage";
import { ShowPage } from "./pages/ShowPage";
import { ShowsPage } from "./pages/ShowsPage";
import { routeHash } from "./router";
import { Shell } from "./Shell";
import { signInView } from "./signin";
import { PillButton } from "./ui/controls";
import { Banner, EmptyState, Panel } from "./ui/layout";
import { useRoute } from "./useRoute";
import { useUploads } from "./useUploads";

export function App() {
  const [mode, setMode] = useState<ModeInfo | null>(null);
  const [drive, setDrive] = useState<DriveInfo | null>(null);
  const [loaded, setLoaded] = useState(false);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [dirty, setDirty] = useState(false);
  const [lost, setLost] = useState(false);
  const [session, setSession] = useState(0);
  const [route, navigate] = useRoute(dirty);
  const uploads = useUploads();

  const load = useCallback(() => {
    setLoadError(null);
    getMode()
      .then(async (next) => {
        setMode(next);
        if (!next.signedIn) return;
        setDrive(await getDrive());
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
  if (!loaded) page = <EmptyState>Loading…</EmptyState>;
  else if (route.page === "server") page = <DrivePage drive={drive} onDrive={onDrive} server={server ? { name: mode.name, onSignedOut: signedOut } : null} />;
  else if (!ready)
    page = (
      <Panel>
        <div className="ui-line">
          <span className="grow">Choose and set up a drive first.</span>
          <PillButton kind="primary" onClick={() => navigate({ page: "server" })}>
            Choose a drive
          </PillButton>
        </div>
      </Panel>
    );
  else if (route.page === "channels") page = <ChannelListPage onNavigate={navigate} />;
  else if (route.page === "ads") page = <AdsPage onNavigate={navigate} />;
  else if (route.page === "shows") page = <ShowsPage onNavigate={navigate} />;
  else if (route.page === "show") page = <ShowPage key={routeHash(route)} channel={route.channel} show={route.show} server={server} onNavigate={navigate} />;
  else if (route.page === "episode") page = <EpisodePage key={route.path} path={route.path} server={server} onNavigate={navigate} />;
  else if (route.page === "ad") page = <AdPage key={route.path} path={route.path} server={server} onNavigate={navigate} />;
  else if (route.page === "folder") page = <FolderPage key={route.folder} folder={route.folder} onNavigate={navigate} />;
  else if (route.page === "channel") page = <ChannelPage key={route.number} number={route.number} tab={route.tab} onNavigate={navigate} onDirty={setDirty} />;
  else if (route.page === "add") page = <AddPage key={routeHash(route)} route={route} server={server} uploads={uploads} onDone={() => navigate({ page: "episodes" })} onNavigate={navigate} />;
  else page = <EpisodesPage key={session} server={server} uploads={uploads} onNavigate={navigate} />;

  return (
    <Shell server={server} route={route} onNavigate={navigate}>
      {loadError !== null && (
        <Banner kind="error">
          <span className="grow">Could not read the drive setting: {loadError}</span>
          <PillButton small onClick={load}>
            Retry
          </PillButton>
        </Banner>
      )}
      {page}
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
    </Shell>
  );
}
