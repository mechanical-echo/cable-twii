import type { Channel } from "../../server/types.js";

export const ZOOMS = [30, 60, 120, 240] as const;

interface Props {
  channels: Channel[];
  channel: Channel;
  seed: number;
  dirty: boolean;
  busy: boolean;
  canUndo: boolean;
  canRedo: boolean;
  pxPerHour: number;
  onChannel: (number: number) => void;
  onUndo: () => void;
  onRedo: () => void;
  onAddSlot: () => void;
  onZoom: (pxPerHour: number) => void;
  onReshuffle: () => void;
  onSave: () => void;
}

export function ScheduleToolbar(props: Props) {
  const { channel, dirty, busy } = props;
  return (
    <>
      <div className="row">
        <h2 className="grow">Schedule</h2>
        {dirty && <span className="unsaved">Unsaved changes</span>}
      </div>
      <div className="row">
        {props.channels.map((c) => (
          <button key={c.number} className={c.number === channel.number ? "pill on" : "pill"} onClick={() => props.onChannel(c.number)}>
            CH{c.number} {c.name}
          </button>
        ))}
        <span className="grow" />
        <span className="pill on" title="Change the mode on the Channels page">
          {channel.mode === "timed" ? "Timed" : "Looping"}
        </span>
      </div>
      <div className="row toolbar">
        <button disabled={!props.canUndo || busy} onClick={props.onUndo}>
          Undo
        </button>
        <button disabled={!props.canRedo || busy} onClick={props.onRedo}>
          Redo
        </button>
        {channel.mode === "timed" && (
          <button disabled={busy} onClick={props.onAddSlot}>
            Add slot
          </button>
        )}
        <span className="zoom">
          {ZOOMS.map((z) => (
            <button key={z} className={z === props.pxPerHour ? "pill on" : "pill"} onClick={() => props.onZoom(z)}>
              {z} px/h
            </button>
          ))}
        </span>
        <span className="grow" />
        <button disabled={dirty || busy} title={dirty ? "Save first" : `Seed ${props.seed}`} onClick={props.onReshuffle}>
          Reshuffle
        </button>
        {dirty && <span className="muted">Save first</span>}
        <button className="primary" disabled={!dirty || busy} onClick={props.onSave}>
          Save
        </button>
      </div>
    </>
  );
}
