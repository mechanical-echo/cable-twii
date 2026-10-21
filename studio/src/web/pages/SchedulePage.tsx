import {
  type CollisionDetection,
  DndContext,
  type DragEndEvent,
  type DragMoveEvent,
  DragOverlay,
  type DragStartEvent,
  PointerSensor,
  pointerWithin,
  useSensor,
  useSensors
} from "@dnd-kit/core";
import { useEffect, useMemo, useState } from "react";
import { effectiveFolder } from "../channels";
import { FailureBanner } from "../FailureBanner";
import {
  type DropTarget,
  type ItemRef,
  UNSAVED_PROMPT,
  addSlot,
  deleteItem,
  dropsBefore,
  insertItem,
  itemAt,
  itemLists,
  moveItem,
  pointerY,
  removeSlot,
  renameSlot,
  replaceItem,
  resolveDrop,
  withSchedule
} from "../schedule/editing";
import { entryLabel, itemFromEntry } from "../schedule/entries";
import { ItemEditor } from "../schedule/ItemEditor";
import { layoutLoop, layoutTimed } from "../schedule/layout";
import { LibraryPanel, type LibraryDragData } from "../schedule/LibraryPanel";
import { errorBlocks, issueWhere } from "../schedule/marks";
import { ScheduleToolbar } from "../schedule/ScheduleToolbar";
import { AddSlotDialog } from "../schedule/SlotHeader";
import { Timeline, type SlotDropData } from "../schedule/Timeline";
import type { BlockDragData } from "../schedule/TimelineBlock";
import { useScheduleEditor } from "../schedule/useScheduleEditor";

type DragData = LibraryDragData | BlockDragData | SlotDropData;

const collide: CollisionDetection = (args) => {
  const hits = pointerWithin(args);
  const blocks = hits.filter((hit) => (hit.data?.droppableContainer?.data.current as DragData | undefined)?.source === "block");
  return blocks.length > 0 ? blocks : hits;
};

function dialogOpen(): boolean {
  return document.querySelector(".overlay") !== null;
}

function isTyping(target: EventTarget | null): boolean {
  return target instanceof HTMLElement && (target.isContentEditable || ["INPUT", "SELECT", "TEXTAREA"].includes(target.tagName));
}

interface Props {
  onDirty: (dirty: boolean) => void;
}

export function SchedulePage({ onDirty }: Props) {
  const editor = useScheduleEditor();
  const { channel, schedule, library, dirty, busy } = editor;
  const [pxPerHour, setPxPerHour] = useState<number>(120);
  const [selected, setSelected] = useState<ItemRef | null>(null);
  const [dragLabel, setDragLabel] = useState<string | null>(null);
  const [hint, setHint] = useState<DropTarget | null>(null);
  const [addingSlot, setAddingSlot] = useState(false);
  const [slotError, setSlotError] = useState<string | null>(null);
  const sensors = useSensors(useSensor(PointerSensor, { activationConstraint: { distance: 4 } }));

  useEffect(() => onDirty(dirty), [dirty, onDirty]);
  useEffect(() => () => onDirty(false), [onDirty]);

  useEffect(() => {
    if (!dirty) return;
    const warn = (event: BeforeUnloadEvent) => {
      event.preventDefault();
      event.returnValue = "";
    };
    window.addEventListener("beforeunload", warn);
    return () => window.removeEventListener("beforeunload", warn);
  }, [dirty]);

  const view = useMemo(() => (channel && schedule ? withSchedule(channel, schedule) : null), [channel, schedule]);
  const selectedItem = schedule && selected ? itemAt(schedule, selected) ?? null : null;
  const selectedLabel = useMemo(() => {
    if (!view || !selected) return "";
    const blocks = view.mode === "timed" ? layoutTimed(view, library) : layoutLoop(view, library).blocks;
    return blocks.find((b) => b.slotIndex === selected.slot && b.itemIndex === selected.item)?.label ?? "";
  }, [view, selected, library]);
  const failure = editor.failure;
  const errors = useMemo(
    () => (failure && channel && schedule && failure.schedule === schedule ? errorBlocks(failure.issues, channel.number, channel.mode) : new Set<string>()),
    [failure, channel, schedule]
  );

  function removeSelected() {
    if (busy || !schedule || !selected || !selectedItem) return;
    editor.edit(deleteItem(schedule, selected));
    setSelected(null);
  }

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (busy || addingSlot || dialogOpen()) return;
      if ((event.key === "Delete" || event.key === "Backspace") && !isTyping(event.target) && selectedItem) {
        event.preventDefault();
        removeSelected();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  });

  function history(step: "undo" | "redo") {
    setSelected(null);
    setSlotError(null);
    if (step === "undo") editor.undo();
    else editor.redo();
  }

  function switchChannel(number: number) {
    if (channel?.number === number) return;
    if (dirty && !window.confirm(UNSAVED_PROMPT)) return;
    setSelected(null);
    setSlotError(null);
    editor.selectChannel(number);
  }

  function target(event: DragMoveEvent | DragEndEvent): DropTarget | null {
    const over = event.over;
    if (!over || !schedule) return null;
    const data = over.data.current as DragData | undefined;
    const rect = event.active.rect.current.translated;
    const y = pointerY(event.activatorEvent, event.delta.y) ?? (rect ? rect.top + rect.height / 2 : 0);
    const lists = itemLists(schedule);
    if (data?.source === "block") return resolveDrop(lists, data.ref, dropsBefore(y, over.rect));
    if (data?.source === "slot") return resolveDrop(lists, { slot: data.slot }, false);
    return null;
  }

  function onDragStart(event: DragStartEvent) {
    const data = event.active.data.current as DragData | undefined;
    if (data?.source === "library") setDragLabel(entryLabel(data.entry));
    if (data?.source === "block") setDragLabel(data.label);
  }

  function onDragMove(event: DragMoveEvent) {
    const next = target(event);
    if (next?.slot !== hint?.slot || next?.index !== hint?.index) setHint(next);
  }

  function onDragEnd(event: DragEndEvent) {
    setDragLabel(null);
    setHint(null);
    if (busy) return;
    const drop = target(event);
    const data = event.active.data.current as DragData | undefined;
    if (!drop || !schedule || !channel || !data) return;
    if (data.source === "library") {
      const next = insertItem(schedule, drop, itemFromEntry(data.entry, effectiveFolder(channel)));
      if (next !== schedule) {
        editor.edit(next);
        setSelected({ slot: drop.slot, item: drop.index });
      }
    } else if (data.source === "block") {
      const moved = moveItem(schedule, data.ref, drop);
      editor.edit(moved.schedule);
      setSelected(moved.ref);
    }
  }

  function slotChange(result: ReturnType<typeof addSlot>): string | null {
    if (!result.ok) return result.error;
    if (schedule?.mode === "timed") editor.edit({ mode: "timed", slots: result.slots });
    setSelected(null);
    return null;
  }

  function deleteSlot(index: number) {
    if (schedule?.mode !== "timed") return;
    const slot = schedule.slots[index];
    if (slot && slot.items.length > 0 && !window.confirm(`Delete slot ${slot.at} and its ${slot.items.length} items?`)) return;
    setSlotError(slotChange(removeSlot(schedule.slots, index)));
  }

  if (editor.loadError !== null) {
    return (
      <div className="banner error row">
        <span className="grow">{editor.loadError}</span>
        <button onClick={editor.load}>Retry</button>
      </div>
    );
  }
  if (!editor.config) return <p className="muted">Loading…</p>;
  if (!channel || !schedule || !view) return <div className="card">No channels yet. Add one on the Channels page.</div>;

  const slotAt = (index: number) => (schedule.mode === "timed" ? schedule.slots[index]?.at : undefined);
  const where = (issue: Parameters<typeof issueWhere>[0]) =>
    issueWhere(issue, issue.channel === channel.number ? channel.mode : null, issue.channel === channel.number ? slotAt : () => undefined);

  return (
    <section className="schedule">
      <ScheduleToolbar
        channels={editor.channels}
        channel={channel}
        seed={editor.config.seed}
        dirty={dirty}
        busy={busy}
        canUndo={editor.canUndo}
        canRedo={editor.canRedo}
        pxPerHour={pxPerHour}
        onChannel={switchChannel}
        onUndo={() => history("undo")}
        onRedo={() => history("redo")}
        onAddSlot={() => setAddingSlot(true)}
        onZoom={setPxPerHour}
        onReshuffle={() => void editor.shuffle()}
        onSave={() => void editor.save()}
      />
      {editor.libraryError !== null && <div className="banner warn">Could not read the library: {editor.libraryError}</div>}
      {slotError !== null && <div className="banner error">{slotError}</div>}
      <FailureBanner failure={failure} onReload={editor.load} where={where} />
      {editor.warnings.length > 0 && (
        <div className="banner warn">
          <ul>
            {editor.warnings.map((issue, i) => (
              <li key={i}>{where(issue) !== "" ? `${where(issue)}: ${issue.message}` : issue.message}</li>
            ))}
          </ul>
        </div>
      )}
      <DndContext
        sensors={sensors}
        collisionDetection={collide}
        onDragStart={onDragStart}
        onDragMove={onDragMove}
        onDragEnd={onDragEnd}
        onDragCancel={() => {
          setDragLabel(null);
          setHint(null);
        }}
      >
        <div className="ed">
          <LibraryPanel library={library} folder={effectiveFolder(channel)} mode={channel.mode} disabled={busy} />
          <div className="pane timeline">
            <Timeline
              channel={view}
              schedule={schedule}
              library={library}
              pxPerHour={pxPerHour}
              selected={selected}
              errors={errors}
              hint={hint}
              busy={busy}
              onSelect={setSelected}
              onRenameSlot={(index, at) => (schedule.mode === "timed" ? slotChange(renameSlot(schedule.slots, index, at)) : null)}
              onDeleteSlot={deleteSlot}
            />
          </div>
          <ItemEditor
            item={selectedItem}
            label={selectedLabel}
            channel={view}
            library={library}
            disabled={busy}
            onChange={(item) => !busy && selected && editor.edit(replaceItem(schedule, selected, item))}
            onDelete={removeSelected}
          />
        </div>
        <DragOverlay dropAnimation={null}>{dragLabel !== null && <div className="drag-ghost">{dragLabel}</div>}</DragOverlay>
      </DndContext>
      {addingSlot && (
        <AddSlotDialog
          onAdd={(at) => (schedule.mode === "timed" ? slotChange(addSlot(schedule.slots, at)) : null)}
          onClose={() => setAddingSlot(false)}
        />
      )}
    </section>
  );
}
