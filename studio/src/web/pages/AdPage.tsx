import { useState } from "react";
import { adChannels, channelFolder } from "../../server/scheduleRefs.js";
import { FailureBanner } from "../FailureBanner";
import { AirsOnChips } from "../library/AirsOn";
import { type Route, parentsOf } from "../router";
import { findAd } from "../shows";
import { Checkbox, PillButton, Select, TextField } from "../ui/controls";
import { Banner, Crumbs, EmptyState, PageHead, Panel } from "../ui/layout";
import { MpegPlayer } from "../ui/MpegPlayer";
import { RemovedList, useConfirm } from "../ui/useConfirm";
import { useLibraryData } from "../useLibraryData";
import { useLibraryEdit } from "../useLibraryEdit";
import { FactList, useFacts } from "./EpisodePage";

function adName(path: string): string {
  return path.slice(path.lastIndexOf("/") + 1).replace(/\.mpg$/i, "");
}

export function AdPage({ path, server, onNavigate }: { path: string; server: boolean; onNavigate: (route: Route) => void }) {
  const { library, config, error, reload } = useLibraryData();
  const facts = useFacts(path);
  const edit = useLibraryEdit(server);
  const [dialog, ask] = useConfirm();
  const [name, setName] = useState<string | null>(null);
  const [where, setWhere] = useState<string | null>(null);
  if (error !== null) return <Banner kind="error">{error}</Banner>;
  if (library === null || config === null) return <EmptyState>Loading…</EmptyState>;
  const crumbs = parentsOf({ page: "ad", path }, () => null);
  const found = findAd(library, path);
  if (found === null) {
    return (
      <section>
        <Crumbs items={crumbs} onNavigate={onNavigate} />
        {edit.notice !== null && <Banner kind="ok">{edit.notice}</Banner>}
        <EmptyState>This ad is not in the library any more.</EmptyState>
      </section>
    );
  }
  const { ad, off } = found;
  const channelName = (n: number) => config.channel.find((c) => c.number === n)?.name ?? `Channel ${n}`;
  const nameNow = name ?? adName(path);
  const whereNow = where ?? ad.channel;
  const changed = nameNow.trim() !== adName(path) || whereNow !== ad.channel;
  const places = [{ value: "", label: "All channels" }, ...[...config.channel].sort((a, b) => a.number - b.number).map((c) => ({ value: channelFolder(c), label: c.name }))];
  if (!places.some((p) => p.value === ad.channel)) places.push({ value: ad.channel, label: ad.channel });

  async function save() {
    const result = await edit.run({ kind: "ad", path, name: nameNow, channel: whereNow });
    if (result === null) return;
    setName(null);
    setWhere(null);
    const target = result.moves.find((m) => m.from === path)?.to;
    if (target !== undefined) onNavigate({ page: "ad", path: target });
    else reload();
  }

  async function switchAir(on: boolean) {
    const result = on
      ? await edit.run({ kind: "off", paths: [path], off: false }, { done: "Back on air." })
      : await edit.run(
          { kind: "off", paths: [path], off: true },
          {
            done: "Switched off.",
            confirm: (dry) =>
              dry.removed.length === 0 ? Promise.resolve(true) : ask({ title: "Switch off this ad?", body: <RemovedList removed={dry.removed} channelName={channelName} />, confirm: "Switch off" }),
          },
        );
    if (result !== null) reload();
  }

  async function remove() {
    const result = await edit.run(
      { kind: "delete", paths: [path] },
      {
        done: "Deleted.",
        confirm: (dry) =>
          ask({
            title: `Delete ${adName(path)}?`,
            body: (
              <>
                <p>{path}</p>
                <RemovedList removed={dry.removed} channelName={channelName} />
                <p>Delete can't be undone.</p>
              </>
            ),
            confirm: "Delete",
            danger: true,
          }),
      },
    );
    if (result !== null) onNavigate({ page: "ads" });
  }

  return (
    <section>
      <Crumbs items={crumbs} onNavigate={onNavigate} />
      <PageHead
        title={adName(path)}
        sub={`Ad · ${places.find((p) => p.value === ad.channel)?.label ?? ad.channel}`}
        actions={<Checkbox checked={!off} disabled={edit.busy} onChange={(on) => void switchAir(on)} label={off ? "Off air" : "On air"} />}
      />
      {edit.notice !== null && <Banner kind="ok">{edit.notice}</Banner>}
      <FailureBanner failure={edit.failure} onReload={reload} />
      <MpegPlayer key={path} path={path} durationMs={ad.duration_ms} aspect="16:9" />
      <Panel title="Airs on">
        <AirsOnChips numbers={adChannels(config, library, path)} config={config} onNavigate={onNavigate} off={off} />
      </Panel>
      <Panel title="Information">
        <div className="ui-settings">
          <TextField label="Name" value={nameNow} onChange={setName} width="full" />
          <Select label="Plays on" value={whereNow} options={places} onChange={setWhere} />
        </div>
        <div className="ui-line">
          <PillButton kind="danger" disabled={edit.busy} onClick={() => void remove()}>
            Delete ad
          </PillButton>
          <span className="grow" />
          <PillButton kind="primary" disabled={!changed || edit.busy} onClick={() => void save()}>
            Save
          </PillButton>
        </div>
      </Panel>
      <Panel title="File">
        <FactList durationMs={ad.duration_ms} facts={facts} path={path} />
      </Panel>
      {dialog}
    </section>
  );
}
