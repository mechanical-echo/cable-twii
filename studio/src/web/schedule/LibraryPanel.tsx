import { useDraggable } from "@dnd-kit/core";
import { type ReactNode, createContext, useContext } from "react";
import type { Mode } from "../../server/types.js";
import { baseName, formatDuration } from "../format";
import { type ChannelNode, type ShowNode, type WebLibrary, buildTree, descriptionKey, seasonLabel } from "../tree";
import { type LibraryEntry, adsFor, entryKey, ruleLabel, rulesFor } from "./entries";

interface Props {
  library: WebLibrary;
  folder: string;
  mode: Mode;
  disabled: boolean;
}

export interface LibraryDragData {
  source: "library";
  entry: LibraryEntry;
}

const DisabledContext = createContext(false);

function Chip({ entry, className, children }: { entry: LibraryEntry; className: string; children: ReactNode }) {
  const disabled = useContext(DisabledContext);
  const data: LibraryDragData = { source: "library", entry };
  const { attributes, listeners, setNodeRef, isDragging } = useDraggable({ id: `lib:${entryKey(entry)}`, data, disabled });
  return (
    <div ref={setNodeRef} className={`${className}${isDragging ? " dragging" : ""}`} {...attributes} {...listeners}>
      {children}
    </div>
  );
}

function Rules({ channel, show, season, mode }: { channel: string; show: string; season: number; mode: Mode }) {
  return (
    <>
      {rulesFor(mode).map((rule) => (
        <Chip key={rule} className="rule" entry={{ kind: "rule", rule, channel, show, season }}>
          {ruleLabel(rule)}
          {season > 0 ? ` · season ${season}` : ""}
        </Chip>
      ))}
    </>
  );
}

function Show({ channel, node, mode }: { channel: string; node: ShowNode; mode: Mode }) {
  const folder = node.show.folder;
  return (
    <details className="lib-show">
      <summary>{node.show.name}</summary>
      <Rules channel={channel} show={folder} season={0} mode={mode} />
      {node.seasons.map((season) => (
        <details key={season.season} className="lib-season">
          <summary>{seasonLabel(season.season)}</summary>
          {season.season > 0 && <Rules channel={channel} show={folder} season={season.season} mode={mode} />}
          {season.episodes.map((ep) => (
            <Chip key={ep.path} className="ep" entry={{ kind: "episode", channel, show: folder, season: ep.season, episode: ep.episode }}>
              <span>
                {descriptionKey(ep)} {ep.title}
              </span>
              <span className="muted">{formatDuration(ep.duration_ms)}</span>
            </Chip>
          ))}
        </details>
      ))}
    </details>
  );
}

function Shows({ node, mode }: { node: ChannelNode | undefined; mode: Mode }) {
  if (!node || node.shows.length === 0) return <p className="muted">No shows yet.</p>;
  return (
    <>
      {node.shows.map((show) => (
        <Show key={show.show.folder} channel={node.channel} node={show} mode={mode} />
      ))}
    </>
  );
}

export function LibraryPanel({ library, folder, mode, disabled }: Props) {
  const tree = buildTree(library);
  const own = tree.find((n) => n.channel === folder);
  const others = tree.filter((n) => n.channel !== folder && n.channel !== "" && n.shows.length > 0);
  const ads = adsFor(library, folder);
  return (
    <DisabledContext.Provider value={disabled}>
      <aside className="pane lib">
        <h4 className="pane-head">Library · {folder}</h4>
        <div className="lib-body">
          <Shows node={own} mode={mode} />
          {others.length > 0 && (
            <details className="lib-group">
              <summary>Other channels</summary>
              {others.map((node) => (
                <details key={node.channel} className="lib-show">
                  <summary>{node.channel}</summary>
                  <Shows node={node} mode={mode} />
                </details>
              ))}
            </details>
          )}
          <details className="lib-group" open>
            <summary>Ads</summary>
            <Chip className="rule ad" entry={{ kind: "ads" }}>
              Random ad
            </Chip>
            {ads.map((ad) => (
              <Chip key={ad.path} className="ep ad" entry={{ kind: "ad", path: ad.path }}>
                <span>{baseName(ad.path).replace(/\.mpg$/, "")}</span>
                <span className="muted">{formatDuration(ad.duration_ms)}</span>
              </Chip>
            ))}
          </details>
        </div>
      </aside>
    </DisabledContext.Provider>
  );
}
