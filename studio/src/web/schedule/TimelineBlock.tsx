import { useSortable } from "@dnd-kit/sortable";
import type { ItemRef } from "./editing";
import { type TimelineBlock as Block, msToPx } from "./layout";
import { describeLength, overlapLabel } from "./marks";

export interface BlockDragData {
  source: "block";
  ref: ItemRef;
  label: string;
}

interface Props {
  id: string;
  block: Block;
  pxPerHour: number;
  selected: boolean;
  error: boolean;
  onSelect: (ref: ItemRef) => void;
}

const MIN_HEIGHT = 4;
const LABEL_HEIGHT = 16;

function kindClass(block: Block): string {
  switch (block.kind) {
    case "episode":
      return "show";
    case "ad":
      return "adb";
    case "ads":
      return "adb est";
    case "fill":
      return "rand fill";
    default:
      return block.exact ? "show" : "rand";
  }
}

export function TimelineBlock({ id, block, pxPerHour, selected, error, onSelect }: Props) {
  const ref: ItemRef = { slot: block.slotIndex, item: block.itemIndex ?? 0 };
  const data: BlockDragData = { source: "block", ref, label: block.label };
  const { attributes, listeners, setNodeRef, isDragging } = useSortable({ id, data });
  const height = Math.max(MIN_HEIGHT, msToPx(block.durationMs, pxPerHour));
  const length = describeLength(block.durationMs, block.exact);
  const classes = ["blk", kindClass(block)];
  if (block.overlapMs > 0) classes.push("over");
  if (error) classes.push("err");
  if (selected) classes.push("sel");
  if (isDragging) classes.push("dragging");
  const top = msToPx(block.startMs, pxPerHour);
  return (
    <>
      <div
        ref={setNodeRef}
        className={classes.join(" ")}
        style={{ top, height }}
        title={`${block.label} · ${length}`}
        onClick={() => onSelect(ref)}
        {...attributes}
        {...listeners}
      >
        {height >= LABEL_HEIGHT && (
          <>
            <b>{block.label}</b>
            <small>{block.kind === "fill" ? "fills the rest of the slot" : length}</small>
          </>
        )}
      </div>
      {block.overlapMs > 0 && (
        <span className="warn" style={{ top: top + height }}>
          {overlapLabel(block.overlapMs)}
        </span>
      )}
    </>
  );
}
