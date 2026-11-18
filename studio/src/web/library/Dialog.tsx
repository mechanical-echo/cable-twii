import type { ReactNode } from "react";

interface Props {
  title: string;
  onClose: () => void;
  busy?: boolean;
  children: ReactNode;
}

export function Dialog({ title, onClose, busy = false, children }: Props) {
  return (
    <div className="overlay">
      <div className="dialog">
        <div className="dialog-head">
          <h3>{title}</h3>
          <button disabled={busy} onClick={onClose}>Close</button>
        </div>
        {children}
      </div>
    </div>
  );
}
