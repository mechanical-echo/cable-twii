import type { Channel, Item } from "../../server/types.js";
import { effectiveFolder } from "../channels";
import type { WebLibrary } from "../tree";
import { MAX_ADS_BETWEEN, MAX_COUNT, setAdsBetween, setCount, setPickType, setSeason } from "./editing";
import { seasonsOf } from "./entries";
import { estimateItemMs } from "./layout";
import { describeLength } from "./marks";

interface Props {
  item: Item | null;
  label: string;
  channel: Channel;
  library: WebLibrary;
  disabled: boolean;
  onChange: (item: Item) => void;
  onDelete: () => void;
}

function NumberField({ label, value, min, max, disabled, onChange }: { label: string; value: number; min: number; max: number; disabled: boolean; onChange: (n: number) => void }) {
  return (
    <label>
      {label} ({min}–{max})
      <input
        className="num"
        type="number"
        min={min}
        max={max}
        value={value}
        disabled={disabled}
        onChange={(e) => {
          if (e.target.value !== "") onChange(Number(e.target.value));
        }}
      />
    </label>
  );
}

export function ItemEditor({ item, label, channel, library, disabled, onChange, onDelete }: Props) {
  if (item === null) {
    return (
      <aside className="pane editor">
        <h4 className="pane-head">Block</h4>
        <p className="muted pad">Click a block to edit it. Drag blocks to reorder, Delete removes the selected one.</p>
      </aside>
    );
  }
  const estimate = estimateItemMs(item, channel, library);
  const hasShow = item.type !== "ads" && item.type !== "ad";
  const seasons = hasShow ? seasonsOf(library, item.from ?? effectiveFolder(channel), item.show).filter((s) => s > 0) : [];
  return (
    <aside className="pane editor">
      <h4 className="pane-head">Block</h4>
      <div className="form-grid column pad">
        <b>{label}</b>
        {hasShow && item.from !== undefined && <span className="muted">From {item.from}</span>}
        {(item.type === "random" || item.type === "random-run") && (
          <label>
            Pick
            <select value={item.type} disabled={disabled} onChange={(e) => onChange(setPickType(item, e.target.value as "random" | "random-run"))}>
              <option value="random">Random episodes</option>
              <option value="random-run">Random sequential run</option>
            </select>
          </label>
        )}
        {(item.type === "random" || item.type === "random-run" || item.type === "ads") && (
          <NumberField label="Count" value={item.count} min={1} max={MAX_COUNT} disabled={disabled} onChange={(n) => onChange(setCount(item, n))} />
        )}
        {(item.type === "random" || item.type === "random-run" || item.type === "fill") && (
          <label>
            Season
            <select value={item.season ?? 0} disabled={disabled} onChange={(e) => onChange(setSeason(item, Number(e.target.value)))}>
              <option value={0}>All seasons</option>
              {[...new Set([...seasons, ...(item.season ? [item.season] : [])])].map((s) => (
                <option key={s} value={s}>
                  Season {s}
                </option>
              ))}
            </select>
          </label>
        )}
        {item.type === "fill" && (
          <NumberField
            label="Ads between"
            value={item.ads_between ?? 0}
            min={0}
            max={MAX_ADS_BETWEEN}
            disabled={disabled}
            onChange={(n) => onChange(setAdsBetween(item, n))}
          />
        )}
        <span className="muted">
          {estimate.exact ? "Length" : "Estimate"}: {describeLength(estimate.ms, estimate.exact)}
        </span>
        <div className="row">
          <button disabled={disabled} onClick={onDelete}>
            🗑 Delete
          </button>
        </div>
      </div>
    </aside>
  );
}
