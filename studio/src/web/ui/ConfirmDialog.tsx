import type { ReactNode } from "react";
import { PillButton } from "./controls";

interface Props {
  title: string;
  body: ReactNode;
  confirm: string;
  danger?: boolean;
  onConfirm: () => void;
  onCancel: () => void;
}

export function ConfirmDialog({ title, body, confirm, danger = false, onConfirm, onCancel }: Props) {
  return (
    <div className="overlay" onKeyDown={(e) => e.key === "Escape" && onCancel()}>
      <div className="ui-dialog" role="dialog" aria-modal="true" aria-label={title}>
        <h2>{title}</h2>
        <div>{body}</div>
        <div className="ui-dialog-actions">
          <PillButton onClick={onCancel}>Cancel</PillButton>
          <PillButton kind={danger ? "danger" : "primary"} onClick={onConfirm}>
            {confirm}
          </PillButton>
        </div>
      </div>
    </div>
  );
}
