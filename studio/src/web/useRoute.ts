import { useCallback, useEffect, useRef, useState } from "react";
import { type Route, leaveAllowed, parseRoute, routeHash } from "./router";
import { UNSAVED_PROMPT } from "./schedule/editing";

export function useRoute(dirty: boolean): [Route, (next: Route) => void] {
  const [route, setRoute] = useState(() => parseRoute(window.location.hash));
  const dirtyRef = useRef(dirty);
  dirtyRef.current = dirty;
  const shown = useRef(window.location.hash);
  const approved = useRef<string | null>(null);

  useEffect(() => {
    function onHash() {
      const hash = window.location.hash;
      if (hash === shown.current) return;
      const ok = approved.current === hash || leaveAllowed(dirtyRef.current, () => window.confirm(UNSAVED_PROMPT));
      approved.current = null;
      if (!ok) {
        history.replaceState(history.state, "", `${window.location.pathname}${window.location.search}${shown.current}`);
        return;
      }
      shown.current = hash;
      setRoute(parseRoute(hash));
    }
    window.addEventListener("hashchange", onHash);
    return () => window.removeEventListener("hashchange", onHash);
  }, []);

  const navigate = useCallback((next: Route) => {
    const hash = routeHash(next);
    if (hash === window.location.hash) return;
    if (!leaveAllowed(dirtyRef.current, () => window.confirm(UNSAVED_PROMPT))) return;
    approved.current = hash;
    window.location.hash = hash;
  }, []);

  return [route, navigate];
}
