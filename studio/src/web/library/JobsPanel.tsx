import { useEffect, useRef, useState } from "react";
import { type Job, getJobs, subscribeJobs } from "../api";
import { baseName } from "../format";

interface Props {
  onFinished: () => void;
}

export function JobsPanel({ onFinished }: Props) {
  const [jobs, setJobs] = useState<Map<string, Job>>(new Map());
  const [problem, setProblem] = useState<string | null>(null);
  const finished = useRef(onFinished);
  finished.current = onFinished;

  useEffect(() => {
    let alive = true;
    const resync = () =>
      getJobs()
        .then((list) => {
          if (!alive) return;
          setJobs(new Map(list.map((j) => [j.id, j] as const)));
          setProblem(null);
        })
        .catch((err: Error) => {
          if (alive) setProblem(`Could not load jobs: ${err.message}`);
        });
    const unsubscribe = subscribeJobs(
      (job) => {
        setJobs((prev) => new Map(prev).set(job.id, job));
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

  const list = [...jobs.values()];
  if (list.length === 0 && problem === null) return null;
  return (
    <div className="card">
      <h3>Conversions</h3>
      {problem !== null && <div className="banner error">{problem}</div>}
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
