import { useState } from "react";
import { type DriveInfo, driveDone, initDrive, logout, pickDrive, setDrive } from "../api";
import { formatBytes } from "../format";

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

  return (
    <section>
      <h2>Server</h2>
      {error !== null && <div className="banner error">{error}</div>}
      <div className="card">
        <h3>{server.name ?? "cable-twii server"}</h3>
        <dl className="facts">
          <dt>Library folder</dt>
          <dd>{drive?.path ?? "…"}</dd>
          <dt>Free space</dt>
          <dd>{drive === null ? "…" : formatBytes(drive.freeBytes)}</dd>
        </dl>
        <button disabled={busy} onClick={signOut}>
          Sign out
        </button>
      </div>
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
      <h2>Drive</h2>
      <div className="card">
        <div className="row">
          <button className="primary" disabled={busy} onClick={choose}>
            Choose drive…
          </button>
          <span className="muted">or</span>
          <input
            className="grow"
            placeholder="/Volumes/MyDrive"
            value={typed}
            onChange={(e) => setTyped(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter" && typed.trim() !== "") void useTyped();
            }}
          />
          <button disabled={busy || typed.trim() === ""} onClick={useTyped}>
            Use this folder
          </button>
        </div>
      </div>
      {error !== null && <div className="banner error">{error}</div>}
      {notice !== null && <div className="banner ok">{notice}</div>}
      {drive === null && <p className="muted">No drive chosen yet.</p>}
      {drive !== null && (
        <div className="card">
          <h3>{drive.path}</h3>
          {!drive.usable && <div className="banner error">{drive.reason ?? "This drive cannot be used."}</div>}
          <dl className="facts">
            <dt>Type</dt>
            <dd>{drive.isVolumeRoot ? "Drive" : "Folder"}{drive.fsType !== null ? ` · ${drive.fsType}` : ""}</dd>
            <dt>Free space</dt>
            <dd>{formatBytes(drive.freeBytes)}</dd>
            <dt>cable-twii folder</dt>
            <dd>{drive.hasRoot ? "Set up" : "Not set up yet"}</dd>
          </dl>
          {drive.usable && !drive.hasRoot && (
            <button className="primary" disabled={busy} onClick={setup}>
              Set up cable-twii on this drive
            </button>
          )}
          {drive.usable && drive.hasRoot && (
            <button className="primary" disabled={busy} onClick={done}>
              Done — safe to eject
            </button>
          )}
        </div>
      )}
    </section>
  );
}
