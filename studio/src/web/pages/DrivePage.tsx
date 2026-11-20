import { useEffect, useState } from "react";
import { type DriveInfo, driveDone, getShrinkCount, initDrive, logout, pickDrive, setDrive, startShrink } from "../api";
import { formatBytes } from "../format";
import { PillButton, TextField } from "../ui/controls";
import { Banner, EmptyState, PageHead, Panel } from "../ui/layout";

export interface ServerInfo {
  name: string | null;
  onSignedOut: () => void;
}

interface Props {
  drive: DriveInfo | null;
  onDrive: (info: DriveInfo | null) => void;
  server: ServerInfo | null;
}

export function DrivePage({ drive, onDrive, server }: Props) {
  return server === null ? <DriveSetup drive={drive} onDrive={onDrive} /> : <ServerPage drive={drive} server={server} />;
}

function ServerPage({ drive, server }: { drive: DriveInfo | null; server: ServerInfo }) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function signOut() {
    setBusy(true);
    setError(null);
    try {
      await logout();
      server.onSignedOut();
    } catch (err) {
      setError((err as Error).message);
      setBusy(false);
    }
  }

  const [shrinkable, setShrinkable] = useState(0);
  const [queued, setQueued] = useState<number | null>(null);

  useEffect(() => {
    getShrinkCount()
      .then((r) => setShrinkable(r.candidates))
      .catch(() => setShrinkable(0));
  }, []);

  async function shrink() {
    setBusy(true);
    setError(null);
    try {
      setQueued((await startShrink()).queued);
      setShrinkable(0);
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <section>
      <PageHead title="Server" />
      {error !== null && <Banner kind="error">{error}</Banner>}
      <Panel title={server.name ?? "cable-twii server"}>
        <dl className="ui-facts">
          <dt>Library folder</dt>
          <dd>{drive?.path ?? "…"}</dd>
          <dt>Free space</dt>
          <dd>{drive === null ? "…" : formatBytes(drive.freeBytes)}</dd>
        </dl>
        <PillButton disabled={busy} onClick={signOut}>
          Sign out
        </PillButton>
      </Panel>
      {queued !== null && shrinkable === 0 && (
        <Banner kind="ok">
          {queued} {queued === 1 ? "video" : "videos"} queued. See Conversions on the Library page.
        </Banner>
      )}
      {shrinkable > 0 && (
        <Panel title="Make older videos smaller for Wi-Fi">
          <p>
            {shrinkable} {shrinkable === 1 ? "video is" : "videos are"} too big to stream reliably over Wi-Fi. They are converted again in place; the Conversions list on the Library page shows the progress.
          </p>
          <PillButton disabled={busy} onClick={shrink}>
            Make smaller
          </PillButton>
        </Panel>
      )}
    </section>
  );
}

function DriveSetup({ drive, onDrive }: Pick<Props, "drive" | "onDrive">) {
  const [typed, setTyped] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [notice, setNotice] = useState<string | null>(null);

  async function run(task: () => Promise<void>) {
    setBusy(true);
    setError(null);
    setNotice(null);
    try {
      await task();
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  const choose = () =>
    run(async () => {
      const info = await pickDrive();
      if (info) onDrive(info);
    });

  const useTyped = () =>
    run(async () => {
      onDrive(await setDrive(typed.trim()));
    });

  const setup = () =>
    run(async () => {
      onDrive(await initDrive());
    });

  const done = () =>
    run(async () => {
      const result = await driveDone();
      setNotice(result.skipped ? "Library saved. This is a plain folder, so there is nothing to eject." : "Done. It is safe to eject the drive.");
    });

  return (
    <section>
      <PageHead title="Drive" />
      <Panel>
        <form
          className="ui-line"
          onSubmit={(e) => {
            e.preventDefault();
            if (typed.trim() !== "") void useTyped();
          }}
        >
          <PillButton kind="primary" disabled={busy} onClick={choose}>
            Choose drive…
          </PillButton>
          <span className="ui-muted">or</span>
          <span className="grow">
            <TextField value={typed} onChange={setTyped} placeholder="/Volumes/MyDrive" width="full" />
          </span>
          <PillButton type="submit" disabled={busy || typed.trim() === ""}>
            Use this folder
          </PillButton>
        </form>
      </Panel>
      {error !== null && <Banner kind="error">{error}</Banner>}
      {notice !== null && <Banner kind="ok">{notice}</Banner>}
      {drive === null && <EmptyState>No drive chosen yet.</EmptyState>}
      {drive !== null && (
        <Panel title={drive.path}>
          {!drive.usable && <Banner kind="error">{drive.reason ?? "This drive cannot be used."}</Banner>}
          <dl className="ui-facts">
            <dt>Type</dt>
            <dd>{drive.isVolumeRoot ? "Drive" : "Folder"}{drive.fsType !== null ? ` · ${drive.fsType}` : ""}</dd>
            <dt>Free space</dt>
            <dd>{formatBytes(drive.freeBytes)}</dd>
            <dt>cable-twii folder</dt>
            <dd>{drive.hasRoot ? "Set up" : "Not set up yet"}</dd>
          </dl>
          {drive.usable && !drive.hasRoot && (
            <PillButton kind="primary" disabled={busy} onClick={setup}>
              Set up cable-twii on this drive
            </PillButton>
          )}
          {drive.usable && drive.hasRoot && (
            <PillButton kind="primary" disabled={busy} onClick={done}>
              Done — safe to eject
            </PillButton>
          )}
        </Panel>
      )}
    </section>
  );
}
