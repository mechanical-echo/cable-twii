import { useRef, useState } from "react";
import { retryJobs } from "../api";
import { type JobRow, homeRows } from "../episodes";
import type { Route } from "../router";
import { PillButton } from "../ui/controls";
import { Banner, EmptyState, ListRow, PageHead, Panel } from "../ui/layout";
import { Chip, ProgressBar } from "../ui/status";
import { useJobs } from "../useJobs";
import { type Uploads, handOff } from "../useUploads";

interface Props {
  server: boolean;
  uploads: Uploads;
  onNavigate: (route: Route) => void;
}

const CHIPS: Record<JobRow["state"], "neutral" | "ok" | "warn" | "error"> = {
  uploading: "neutral",
  queued: "neutral",
  running: "neutral",
  done: "ok",
  failed: "error",
  missing: "warn",
  rejected: "warn",
};

function Row({ row, onDismiss }: { row: JobRow; onDismiss?: () => void }) {
  return (
    <ListRow>
      <div className="grow">
        <div className="ui-job-title">
          {row.title}
          {row.channel !== "" && <small> · {row.channel}</small>}
        </div>
        {row.error !== null && <div className="ui-job-error">{row.error}</div>}
      </div>
      {row.progress !== null && <ProgressBar value={row.progress} />}
      <Chip kind={CHIPS[row.state]}>{row.detail}</Chip>
      {onDismiss !== undefined && (
        <PillButton small onClick={onDismiss}>
          Dismiss
        </PillButton>
      )}
    </ListRow>
  );
}

function DropZone({ server, onNavigate }: Pick<Props, "server" | "onNavigate">) {
  const input = useRef<HTMLInputElement>(null);
  const [over, setOver] = useState(false);
  const take = (files: File[]) => {
    if (files.length === 0) return;
    handOff({ files });
    onNavigate({ page: "add", to: "episodes", channel: null });
  };
  if (!server) {
    return (
      <div className="ui-drop static">
        <b>Add episodes</b>
        <small>Pick a folder with videos on this computer</small>
        <PillButton
          kind="primary"
          onClick={() => {
            handOff({ from: "pick" });
            onNavigate({ page: "add", to: "episodes", channel: null });
          }}
        >
          Pick a folder
        </PillButton>
      </div>
    );
  }
  return (
    <div
      className={over ? "ui-drop over" : "ui-drop"}
      role="button"
      tabIndex={0}
      onClick={() => input.current?.click()}
      onKeyDown={(e) => {
        if (e.key === "Enter" || e.key === " ") {
          e.preventDefault();
          input.current?.click();
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
        take(Array.from(e.dataTransfer.files));
      }}
    >
      <b>Add episodes</b>
      <small>Drop video files here, or click to pick them</small>
      <span
        onClick={(e) => e.stopPropagation()}
        onKeyDown={(e) => e.stopPropagation()}
      >
        <PillButton
          onClick={() => {
            handOff({ from: "imports" });
            onNavigate({ page: "add", to: "episodes", channel: null });
          }}
        >
          Use the imports folder
        </PillButton>
      </span>
      <input
        ref={input}
        type="file"
        multiple
        accept="video/*"
        hidden
        onChange={(e) => {
          const files = Array.from(e.target.files ?? []);
          e.target.value = "";
          take(files);
        }}
      />
    </div>
  );
}

export function EpisodesPage({ server, uploads, onNavigate }: Props) {
  const { jobs, problem } = useJobs();
  const [retrying, setRetrying] = useState(false);
  const [retryError, setRetryError] = useState<string | null>(null);
  const { active, recent } = homeRows(jobs, uploads.tasks, Date.now());
  const uploading = uploads.tasks.some((t) => t.state === "waiting" || t.state === "uploading");
  const anyFailed = recent.some((r) => r.state === "failed");

  const retry = () => {
    setRetrying(true);
    setRetryError(null);
    retryJobs()
      .catch((err: Error) => setRetryError(`Could not retry: ${err.message}`))
      .finally(() => setRetrying(false));
  };

  return (
    <section>
      <PageHead
        title="Episodes"
        actions={
          <PillButton kind="primary" onClick={() => onNavigate({ page: "add", to: "episodes", channel: null })}>
            Add episodes
          </PillButton>
        }
      />
      <DropZone server={server} onNavigate={onNavigate} />
      {uploading && <Banner kind="warn">Keep this tab open until uploads finish</Banner>}
      {problem !== null && <Banner kind="error">{problem}</Banner>}
      {retryError !== null && <Banner kind="error">{retryError}</Banner>}
      {active.length === 0 && recent.length === 0 && <EmptyState>No episodes yet. Add some to get started.</EmptyState>}
      {active.length > 0 && (
        <Panel title="In progress">
          {active.map((row) => (
            <Row key={row.key} row={row} />
          ))}
        </Panel>
      )}
      {recent.length > 0 && (
        <Panel
          title="Recent"
          actions={
            anyFailed ? (
              <PillButton small disabled={retrying} onClick={retry}>
                Retry failed
              </PillButton>
            ) : undefined
          }
        >
          {recent.map((row) => (
            <Row key={row.key} row={row} onDismiss={row.state === "rejected" ? () => uploads.dismiss(row.key.slice("upload:".length)) : undefined} />
          ))}
        </Panel>
      )}
    </section>
  );
}
