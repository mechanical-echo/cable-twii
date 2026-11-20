import type { ReactNode } from "react";

export function Chip({ kind = "neutral", children }: { kind?: "neutral" | "ok" | "warn" | "error"; children: ReactNode }) {
  return <span className={kind === "neutral" ? "ui-chip" : `ui-chip ${kind}`}>{children}</span>;
}

export function ProgressBar({ value }: { value: number }) {
  const percent = Math.round(Math.min(1, Math.max(0, value)) * 100);
  return (
    <span className="ui-progress" role="progressbar" aria-valuemin={0} aria-valuemax={100} aria-valuenow={percent}>
      <i style={{ width: `${percent}%` }} />
    </span>
  );
}
