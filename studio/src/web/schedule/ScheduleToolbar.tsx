import type { Channel } from "../../server/types.js";
import { PillButton } from "../ui/controls";
import { Chip } from "../ui/status";

export const ZOOMS = [30, 60, 120, 240] as const;

interface Props {
  channel: Channel;
  seed: number;
  dirty: boolean;
  busy: boolean;
  canUndo: boolean;
  canRedo: boolean;
  pxPerHour: number;
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
    <div className="ui-line ui-sched-bar">
      <PillButton small disabled={!props.canUndo || busy} onClick={props.onUndo}>
        Undo
      </PillButton>
      <PillButton small disabled={!props.canRedo || busy} onClick={props.onRedo}>
        Redo
      </PillButton>
      {channel.mode === "timed" && (
        <PillButton small disabled={busy} onClick={props.onAddSlot}>
          Add slot
        </PillButton>
      )}
      <span className="ui-zoom" role="group" aria-label="Zoom">
        {ZOOMS.map((z) => (
          <button key={z} className={z === props.pxPerHour ? "ui-tab on" : "ui-tab"} onClick={() => props.onZoom(z)}>
            {z} px/h
          </button>
        ))}
      </span>
      <span className="grow" />
      {dirty && <Chip kind="warn">Unsaved changes</Chip>}
      <PillButton small disabled={dirty || busy} onClick={props.onReshuffle}>
        {dirty ? "Save before reshuffling" : "Reshuffle"}
      </PillButton>
      <PillButton kind="primary" disabled={!dirty || busy} onClick={props.onSave}>
        Save
      </PillButton>
    </div>
  );
}
