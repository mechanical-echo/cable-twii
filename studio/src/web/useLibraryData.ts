import { useCallback, useEffect, useState } from "react";
import { type Config, getConfig, getLibrary } from "./api";
import type { WebLibrary } from "./tree";

export function useLibraryData(): { library: WebLibrary | null; config: Config | null; mtimeMs: number; error: string | null; reload: () => void } {
  const [library, setLibrary] = useState<WebLibrary | null>(null);
  const [loaded, setLoaded] = useState<{ config: Config; mtimeMs: number } | null>(null);
  const [error, setError] = useState<string | null>(null);

  const reload = useCallback(() => {
    setError(null);
    Promise.all([getLibrary(), getConfig()])
      .then(([nextLibrary, nextConfig]) => {
        setLibrary(nextLibrary);
        setLoaded(nextConfig);
      })
      .catch((err: Error) => setError(err.message));
  }, []);

  useEffect(reload, [reload]);

  return { library, config: loaded?.config ?? null, mtimeMs: loaded?.mtimeMs ?? 0, error, reload };
}
