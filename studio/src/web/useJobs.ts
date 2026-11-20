import { useEffect, useRef, useState } from "react";
import { type Job, getJobs, subscribeJobs } from "./api";
import { mergeResync, streamProblem } from "./jobs";

export function useJobs(onFinished?: () => void): { jobs: Job[]; problem: string | null } {
  const [jobs, setJobs] = useState<Map<string, Job>>(new Map());
  const [problem, setProblem] = useState<string | null>(null);
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
          if (merged.finished) finished.current?.();
        })
        .catch((err: Error) => {
          if (alive) setProblem(`Could not load conversions: ${err.message}`);
        })
        .finally(() => pending.delete(seen));
    };
    const unsubscribe = subscribeJobs(
      (job) => {
        current.set(job.id, job);
        for (const seen of pending) seen.add(job.id);
        setJobs(new Map(current));
        if (job.state === "done") finished.current?.();
      },
      {
        onOpen: () => void resync(),
        onError: (closed) => {
          if (alive) setProblem(streamProblem(closed));
        },
      },
    );
    return () => {
      alive = false;
      unsubscribe();
    };
  }, []);

  return { jobs: [...jobs.values()], problem };
}
