import { useState } from "react";
import type { Config } from "../api";
import { PillButton, Select } from "../ui/controls";

interface Props {
  config: Config;
  channel?: number;
  title: string;
  confirm: string;
  onPick: (number: number, slot: number | null) => void;
  onCancel: () => void;
}

export function SlotPicker({ config, channel, title, confirm, onPick, onCancel }: Props) {
  const channels = [...config.channel].sort((a, b) => a.number - b.number);
  const [number, setNumber] = useState(channel ?? channels[0]?.number ?? 0);
  const chosen = config.channel.find((c) => c.number === number);
  const slots = chosen?.mode === "timed" ? (chosen.slot ?? []) : [];
  const [slot, setSlot] = useState<number | null>(null);
  const slotNow = slots.length === 0 ? null : (slot ?? slots.length - 1);
  return (
    <div className="overlay">
      <div className="ui-dialog" role="dialog" aria-modal="true" aria-label={title}>
        <h2>{title}</h2>
        <div className="ui-stack">
          {channel === undefined && (
            <Select
              label="Channel"
              value={String(number)}
              options={channels.map((c) => ({ value: String(c.number), label: `${c.number} · ${c.name}` }))}
              onChange={(v) => {
                setNumber(Number(v));
                setSlot(null);
              }}
            />
          )}
          {chosen?.mode === "timed" && slots.length > 0 && (
            <Select label="At the end of slot" value={String(slotNow)} options={slots.map((s, i) => ({ value: String(i), label: s.at }))} onChange={(v) => setSlot(Number(v))} />
          )}
          {chosen?.mode === "timed" && slots.length === 0 && <p className="ui-muted">This channel has no slots yet. Add one in its Schedule tab first.</p>}
          {chosen?.mode === "looping" && <p className="ui-muted">It goes at the end of the loop.</p>}
        </div>
        <div className="ui-dialog-actions">
          <PillButton onClick={onCancel}>Cancel</PillButton>
          <PillButton kind="primary" disabled={chosen === undefined || (chosen.mode === "timed" && slotNow === null)} onClick={() => onPick(number, slotNow)}>
            {confirm}
          </PillButton>
        </div>
      </div>
    </div>
  );
}
