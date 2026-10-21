import { useDroppable } from "@dnd-kit/core";
import { SortableContext, type SortingStrategy } from "@dnd-kit/sortable";
import { useMemo } from "react";
import type { Channel } from "../../server/types.js";
import type { WebLibrary } from "../tree";
import { type DropTarget, type ItemRef, type Schedule, itemId, itemLists } from "./editing";
import { type TimelineBlock as Block, layoutLoop, layoutTimed, msToPx } from "./layout";
import { atToMs, blockKey, describeLength, hintMs, hourLabels } from "./marks";
import { SlotHeader } from "./SlotHeader";
import { TimelineBlock } from "./TimelineBlock";

interface Props {
  channel: Channel;
  schedule: Schedule;
  library: WebLibrary;
  pxPerHour: number;
  selected: ItemRef | null;
  errors: Set<string>;
  hint: DropTarget | null;
  busy: boolean;
  onSelect: (ref: ItemRef) => void;
  onRenameSlot: (index: number, at: string) => string | null;
  onDeleteSlot: (index: number) => void;
}

export interface SlotDropData {
  source: "slot";
  slot: number;
}

const DAY_MS = 24 * 3600000;
const LOOP_TAIL_PX = 48;
const noShift: SortingStrategy = () => null;

function SlotZone({ slot, top, height }: { slot: number; top: number; height: number }) {
  const data: SlotDropData = { source: "slot", slot };
  const { setNodeRef, isOver } = useDroppable({ id: `slot-${slot}`, data });
  return <div ref={setNodeRef} className={isOver ? "slot-zone over" : "slot-zone"} style={{ top, height }} />;
}

function Band({ block, pxPerHour }: { block: Block; pxPerHour: number }) {
  const height = msToPx(block.durationMs, pxPerHour);
  return (
    <div className={`band ${block.kind}`} style={{ top: msToPx(block.startMs, pxPerHour), height }} title={`${block.label} · ${describeLength(block.durationMs, false)}`}>
      {height >= 14 && <small>{block.label}</small>}
    </div>
  );
}

function Blocks(props: Props & { blocks: Block[]; slot: number }) {
  const lists = itemLists(props.schedule);
  const items = lists[props.slot] ?? [];
  const ids = items.map(itemId);
  return (
    <SortableContext items={ids} strategy={noShift}>
      {props.blocks
        .filter((b) => b.slotIndex === props.slot && b.itemIndex !== null)
        .map((block) => {
          const index = block.itemIndex ?? 0;
          return (
            <TimelineBlock
              key={ids[index]}
              id={ids[index]!}
              block={block}
              pxPerHour={props.pxPerHour}
              selected={props.selected?.slot === props.slot && props.selected.item === index}
              error={props.errors.has(blockKey(props.slot, index))}
              onSelect={props.onSelect}
            />
          );
        })}
    </SortableContext>
  );
}

function Hint({ top }: { top: number }) {
  return <div className="drop-hint" style={{ top }} />;
}

function TimedTimeline(props: Props) {
  const { schedule, channel, library, pxPerHour, hint } = props;
  const blocks = useMemo(() => layoutTimed(channel, library), [channel, library]);
  if (schedule.mode !== "timed") return null;
  const slots = schedule.slots;
  if (slots.length === 0) return <p className="muted">No slots yet. Add one to start the day.</p>;
  const origin = atToMs(slots[0]!.at);
  const starts = slots.map((s) => atToMs(s.at) - origin);
  const ends = starts.map((_, i) => starts[i + 1] ?? DAY_MS);
  const last = blocks.reduce((max, b) => Math.max(max, b.startMs + (Number.isFinite(b.durationMs) ? b.durationMs : 0)), DAY_MS);
  const height = msToPx(last, pxPerHour) + 24;
  const hourPx = msToPx(3600000, pxPerHour);
  return (
    <div className="tl" style={{ height }}>
      <div className="hours">
        {hourLabels(slots[0]!.at).map((label, i) => (
          <div key={i} style={{ height: hourPx }}>
            {label}
          </div>
        ))}
      </div>
      <div className="track" style={{ backgroundSize: `100% ${hourPx}px` }}>
        {slots.map((slot, i) => (
          <SlotZone key={`zone-${slot.at}`} slot={i} top={msToPx(starts[i]!, pxPerHour)} height={msToPx(ends[i]! - starts[i]!, pxPerHour)} />
        ))}
        {blocks
          .filter((b) => b.itemIndex === null)
          .map((b) => (
            <Band key={`band-${b.slotIndex}`} block={b} pxPerHour={pxPerHour} />
          ))}
        {slots.map((slot, i) => (
          <Blocks key={`blocks-${slot.at}`} {...props} blocks={blocks} slot={i} />
        ))}
        {slots.map((slot, i) => (
          <SlotHeader
            key={`head-${i}`}
            at={slot.at}
            top={msToPx(starts[i]!, pxPerHour)}
            disabled={props.busy}
            onRename={(at) => props.onRenameSlot(i, at)}
            onDelete={() => props.onDeleteSlot(i)}
          />
        ))}
        {hint !== null && <Hint top={msToPx(hintMs(blocks, hint.slot, hint.index, starts[hint.slot] ?? 0), pxPerHour)} />}
      </div>
    </div>
  );
}

function LoopTimeline(props: Props) {
  const { channel, library, pxPerHour, hint } = props;
  const { blocks, loopMs, exact } = useMemo(() => layoutLoop(channel, library), [channel, library]);
  const height = msToPx(loopMs, pxPerHour) + LOOP_TAIL_PX;
  return (
    <>
      <div className="loop-total">
        Loop · {describeLength(loopMs, exact)} total, repeated all day
      </div>
      <div className="tl loop" style={{ height }}>
        <div className="track" style={{ backgroundSize: `100% ${msToPx(3600000, pxPerHour)}px` }}>
          <SlotZone slot={0} top={0} height={height} />
          <Blocks {...props} blocks={blocks} slot={0} />
          {hint !== null && <Hint top={msToPx(hintMs(blocks, 0, hint.index, 0), pxPerHour)} />}
        </div>
      </div>
    </>
  );
}

export function Timeline(props: Props) {
  return props.schedule.mode === "timed" ? <TimedTimeline {...props} /> : <LoopTimeline {...props} />;
}
