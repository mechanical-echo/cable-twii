import { useEffect, useRef, useState } from "react";
import { type Job, getJobs, subscribeJobs } from "../api";
import { baseName } from "../format";

interface Props {
  onFinished: () => void;
}

export function JobsPanel({ onFinished }: Props) {
  const [jobs, setJobs] = useState<Map<string, Job>>(new Map());
  const finished = useRef(onFinished);
  finished.current = onFinished;

  useEffect(() => {
    let alive = true;
    const unsubscribe = subscribeJobs((job) => {
      setJobs((prev) => new Map(prev).set(job.id, job));
      if (job.state === "done") finished.current();
    });
    getJobs()
      .then((list) => {
        if (!alive) return;
        setJobs((prev) => {
          const next = new Map(list.map((j) => [j.id, j] as const));
          for (const [id, job] of prev) next.set(id, job);
          return next;
        });
      })
      .catch(() => undefined);
    return () => {
      alive = false;
      unsubscribe();
    };
  }, []);

  const list = [...jobs.values()];
  if (list.length === 0) return null;
  return (
    <div className="card">
      <h3>Conversions</h3>
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
