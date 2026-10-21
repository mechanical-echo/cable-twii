import { useEffect, useRef, useState } from "react";
import { type Job, getJobs, retryJobs, subscribeJobs } from "../api";
import { baseName } from "../format";
import { mergeResync } from "../jobs";

interface Props {
  onFinished: () => void;
}

export function JobsPanel({ onFinished }: Props) {
  const [jobs, setJobs] = useState<Map<string, Job>>(new Map());
  const [problem, setProblem] = useState<string | null>(null);
  const [retrying, setRetrying] = useState(false);
  const finished = useRef(onFinished);
  finished.current = onFinished;

  useEffect(() => {
    let alive = true;
    const current = new Map<string, Job>();
    const pending = new Set<Set<string>>();
    const resync = () => {
      const seen = new Set<string>();
      pending.add(seen);
      return getJobs()
        .then((list) => {
          if (!alive) return;
          const merged = mergeResync(current, list, seen);
          current.clear();
          for (const [id, job] of merged.jobs) current.set(id, job);
          setJobs(new Map(current));
          setProblem(null);
          if (merged.finished) finished.current();
        })
        .catch((err: Error) => {
          if (alive) setProblem(`Could not load jobs: ${err.message}`);
        })
        .finally(() => pending.delete(seen));
    };
    const unsubscribe = subscribeJobs(
      (job) => {
        current.set(job.id, job);
        for (const seen of pending) seen.add(job.id);
        setJobs(new Map(current));
        if (job.state === "done") finished.current();
      },
      {
        onOpen: () => void resync(),
        onError: () => {
          if (alive) setProblem("Lost connection to job updates. Reconnecting…");
        },
      },
    );
    return () => {
      alive = false;
      unsubscribe();
    };
  }, []);

  const retry = () => {
    setRetrying(true);
    retryJobs()
      .then(() => setProblem(null))
      .catch((err: Error) => setProblem(`Could not retry: ${err.message}`))
      .finally(() => setRetrying(false));
  };

  const list = [...jobs.values()];
  if (list.length === 0 && problem === null) return null;
  const anyFailed = list.some((job) => job.state === "failed");
  return (
    <div className="card">
      <h3>Conversions</h3>
      {problem !== null && <div className="banner error">{problem}</div>}
      {anyFailed && (
        <button onClick={retry} disabled={retrying}>
          Retry failed
        </button>
      )}
      {list.map((job) => (
        <div key={job.id} className="job">
          <div className="job-head">
            <span>{baseName(job.target)}</span>
            <span className={`state ${job.state}`}>{job.state}</span>
          </div>
          <progress max={1} value={job.state === "done" ? 1 : job.progress} />
          {job.state === "failed" && job.error !== undefined && <div className="banner error">{job.error}</div>}
        </div>
      ))}
    </div>
  );
}
