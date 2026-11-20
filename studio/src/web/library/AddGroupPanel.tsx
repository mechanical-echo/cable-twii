import { useState } from "react";
import type { AddGroup, AddRow, RowNote } from "../adding";
import { NEW_CHANNEL } from "../channels";
import { formatDuration } from "../format";
import { Checkbox, Select, TextField } from "../ui/controls";
import { Panel } from "../ui/layout";
import { Chip } from "../ui/status";

const NEW_SHOW = "\u0000show";

export interface GroupEdit {
  group: AddGroup;
  newChannel: string;
  icon: File | null;
}

interface Props {
  edit: GroupEdit;
  ads: boolean;
  channels: { value: string; label: string }[];
  shows: { value: string; label: string }[];
  onChannel: (channel: string) => void;
  onShow: (show: string, isNew: boolean) => void;
  onNewChannel: (name: string) => void;
  onIcon: (icon: File | null) => void;
  onRow: (id: string, change: Partial<AddRow>) => void;
  onPreview: (row: AddRow) => void;
}

function noteChip(row: AddRow): { text: string; kind: "warn" | "error" | "neutral" } | null {
  const notes: Record<Exclude<RowNote, null>, { text: string; kind: "warn" | "error" | "neutral" }> = {
    "on-air": { text: row.replaces !== null ? "Already on air · Replace?" : "Already queued", kind: "warn" },
    "in-batch": { text: "Already in this list", kind: "warn" },
    added: { text: "Already added", kind: "neutral" },
    "too-short": { text: "Too short", kind: "error" },
    unreadable: { text: "Can't read this file", kind: "error" },
  };
  return row.note === null ? null : notes[row.note];
}

function whole(text: string): number | null {
  const t = text.trim();
  return /^\d+$/.test(t) ? Number(t) : null;
}

function NumberCell({ value, label, onChange }: { value: number | null; label: string; onChange: (v: number | null) => void }) {
  const [text, setText] = useState(value === null ? "" : String(value));
  return (
    <input
      className="ui-input tiny"
      aria-label={label}
      inputMode="numeric"
      value={text}
      onChange={(e) => {
        setText(e.target.value);
        onChange(whole(e.target.value));
      }}
    />
  );
}

function Row({ row, ads, onRow, onPreview }: { row: AddRow; ads: boolean; onRow: Props["onRow"]; onPreview: Props["onPreview"] }) {
  const [open, setOpen] = useState(false);
  const chip = noteChip(row);
  const fixedTitle = row.replaces !== null && row.ticked;
  return (
    <>
      <tr className={row.ticked ? undefined : "off"}>
        <td>
          <Checkbox checked={row.ticked} ariaLabel={`Add ${row.name}`} onChange={(ticked) => onRow(row.id, { ticked })} />
        </td>
        <td>
          <button className="ui-thumb" aria-label={`Preview ${row.name}`} onClick={() => onPreview(row)} />
        </td>
        {!ads && (
          <td className="ui-code">
            <span className="ui-se">
              S<NumberCell value={row.season} label={`Season of ${row.name}`} onChange={(season) => onRow(row.id, { season })} />
              E<NumberCell value={row.episode} label={`Episode of ${row.name}`} onChange={(episode) => onRow(row.id, { episode })} />
            </span>
            <div className="ui-file">{row.name}</div>
          </td>
        )}
        <td className="grow">
          {ads ? (
            <div className="ui-file-main">{row.name}</div>
          ) : (
            <>
              <TextField value={row.title} onChange={(title) => onRow(row.id, { title })} disabled={fixedTitle} width="full" />
              <button className="ui-link" onClick={() => setOpen(!open)}>
                {open ? "Hide description" : row.description !== "" ? "Edit description" : "Add a description"}
              </button>
            </>
          )}
        </td>
        <td className="ui-length">{row.durationMs === null ? "—" : formatDuration(row.durationMs)}</td>
        <td className="ui-note">{chip !== null && <Chip kind={chip.kind}>{chip.text}</Chip>}</td>
      </tr>
      {open && !ads && (
        <tr className="ui-desc">
          <td colSpan={6}>
            <TextField value={row.description} onChange={(description) => onRow(row.id, { description })} placeholder="Description shown on the Wii" width="full" />
          </td>
        </tr>
      )}
    </>
  );
}

export function AddGroupPanel({ edit, ads, channels, shows, onChannel, onShow, onNewChannel, onIcon, onRow, onPreview }: Props) {
  const { group } = edit;
  const { dest } = group;
  const showValue = dest.isNewShow ? NEW_SHOW : dest.show;
  return (
    <Panel>
      <div className="ui-dest">
        <span className="ui-muted">They go to</span>
        <Select label="Channel" value={dest.channel} options={channels} onChange={onChannel} />
        {dest.channel === NEW_CHANNEL && <TextField label="New channel name" value={edit.newChannel} onChange={onNewChannel} width="short" />}
        {!ads && (
          <>
            <Select
              label="Show"
              value={showValue}
              options={[...shows, { value: NEW_SHOW, label: "New show…" }]}
              onChange={(value) => (value === NEW_SHOW ? onShow(dest.show, true) : onShow(value, false))}
            />
            {dest.isNewShow && (
              <>
                <TextField label="New show name" value={dest.show} onChange={(name) => onShow(name, true)} width="short" />
                <label className="ui-field">
                  <span className="ui-label">Icon (optional)</span>
                  <input className="ui-file-input" type="file" accept="image/*" onChange={(e) => onIcon(e.target.files?.[0] ?? null)} />
                </label>
              </>
            )}
          </>
        )}
      </div>
      <table className="ui-rows">
        <thead>
          <tr>
            <th />
            <th>Preview</th>
            {!ads && <th>Episode</th>}
            <th>{ads ? "File" : "Title"}</th>
            <th>Length</th>
            <th />
          </tr>
        </thead>
        <tbody>
          {group.rows.map((row) => (
            <Row key={row.id} row={row} ads={ads} onRow={onRow} onPreview={onPreview} />
          ))}
        </tbody>
      </table>
      {group.rows.some((r) => r.replaces !== null && r.ticked) && <p className="ui-muted">Ticked “Replace” rows keep the episode's file name and title; the old file stays on air until the new one is converted.</p>}
      {dest.isNewShow && dest.show.trim() === "" && <p className="ui-muted">Give the new show a name.</p>}
    </Panel>
  );
}
