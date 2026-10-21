import { useEffect, useState } from "react";
import { Dialog } from "../library/Dialog";

interface Props {
  at: string;
  top: number;
  disabled: boolean;
  onRename: (at: string) => string | null;
  onDelete: () => void;
}

export function SlotHeader({ at, top, disabled, onRename, onDelete }: Props) {
  const [value, setValue] = useState(at);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    setValue(at);
    setError(null);
  }, [at]);

  function commit() {
    if (value.trim() === at) {
      setValue(at);
      setError(null);
      return;
    }
    const problem = onRename(value);
    setError(problem);
    if (problem !== null) setValue(at);
  }

  return (
    <div className="slotline" style={{ top }}>
      <div className="slot-label">
        <span>slot</span>
        <input
          aria-label="Slot start time"
          className="slot-at"
          value={value}
          disabled={disabled}
          onChange={(e) => setValue(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => {
            if (e.key === "Enter") e.currentTarget.blur();
            if (e.key === "Escape") {
              setValue(at);
              setError(null);
            }
          }}
        />
        <button className="mini" disabled={disabled} title="Delete slot" onClick={onDelete}>
          ×
        </button>
        {error !== null && <span className="slot-error">{error}</span>}
      </div>
    </div>
  );
}

export function AddSlotDialog({ onAdd, onClose }: { onAdd: (at: string) => string | null; onClose: () => void }) {
  const [value, setValue] = useState("");
  const [error, setError] = useState<string | null>(null);

  function add() {
    const problem = onAdd(value);
    setError(problem);
    if (problem === null) onClose();
  }

  return (
    <Dialog title="Add slot" onClose={onClose}>
      <label className="desc">
        Start time (HH:MM, 00:00–23:59)
        <input autoFocus placeholder="07:30" value={value} onChange={(e) => setValue(e.target.value)} onKeyDown={(e) => e.key === "Enter" && add()} />
      </label>
      {error !== null && <div className="banner error">{error}</div>}
      <div className="row end">
        <button onClick={onClose}>Cancel</button>
        <button className="primary" onClick={add}>
          Add
        </button>
      </div>
    </Dialog>
  );
}
