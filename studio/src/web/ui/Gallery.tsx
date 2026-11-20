import { useState } from "react";
import { ConfirmDialog } from "./ConfirmDialog";
import { Checkbox, PillButton, Select, TextField } from "./controls";
import { Banner, EmptyState, ListRow, PageHead, Panel } from "./layout";
import { Chip, ProgressBar } from "./status";
import { Tabs } from "./Tabs";

type Tab = "today" | "schedule" | "shows" | "settings";

export function Gallery() {
  const [text, setText] = useState("The Pest");
  const [mode, setMode] = useState<"timed" | "looping">("looping");
  const [ticked, setTicked] = useState(true);
  const [tab, setTab] = useState<Tab>("today");
  const [confirming, setConfirming] = useState(false);
  return (
    <div className="ui-gallery">
      <PageHead title="Components" sub="Every shared piece in each of its variants" actions={<PillButton kind="primary">Add episodes</PillButton>} />
      <Panel title="Pills" actions={<PillButton small>Small</PillButton>}>
        <div className="ui-gallery-line">
          <PillButton>Plain</PillButton>
          <PillButton kind="primary">Primary</PillButton>
          <PillButton kind="danger" onClick={() => setConfirming(true)}>Danger</PillButton>
          <PillButton disabled>Disabled</PillButton>
          <PillButton kind="primary" small>Small primary</PillButton>
        </div>
      </Panel>
      <Panel title="Fields">
        <div className="ui-gallery-line">
          <Select value={mode} options={[{ value: "timed", label: "Timed" }, { value: "looping", label: "Looping" }]} onChange={setMode} />
          <Select label="Mode" value={mode} options={[{ value: "timed", label: "Timed" }, { value: "looping", label: "Looping" }]} onChange={setMode} />
          <Select value={mode} options={[{ value: "timed", label: "Timed" }, { value: "looping", label: "Looping" }]} onChange={setMode} disabled />
          <TextField value={text} onChange={setText} />
          <TextField label="Title" value={text} onChange={setText} width="short" />
          <TextField label="Password" value="secret" onChange={() => {}} type="password" />
          <TextField label="Date" value="2026-11-20" onChange={() => {}} type="date" />
          <TextField value="" placeholder="Disabled" onChange={() => {}} disabled />
          <Checkbox checked={ticked} onChange={setTicked} label="Ads on" />
          <Checkbox checked={ticked} onChange={setTicked} />
          <Checkbox checked={false} onChange={() => {}} label="Disabled" disabled />
        </div>
        <div className="ui-gallery-line" style={{ marginTop: 10 }}>
          <TextField label="Full width" value={text} onChange={setText} width="full" />
        </div>
      </Panel>
      <Panel title="Rows">
        <ListRow>
          <span style={{ flex: 1 }}>Gumball S03E19 · Cartoon Network</span>
          <ProgressBar value={0.62} />
          <Chip>62%</Chip>
        </ListRow>
        <ListRow selected onClick={() => {}}>
          <span style={{ flex: 1 }}>Selected and clickable</span>
          <Chip kind="ok">18 on air</Chip>
        </ListRow>
        <ListRow dimmed>
          <span style={{ flex: 1 }}>Dimmed</span>
          <Chip kind="warn">No shows yet</Chip>
          <Chip kind="error">Failed</Chip>
        </ListRow>
      </Panel>
      <Banner kind="error">Something went wrong.</Banner>
      <Banner kind="warn">Keep this tab open until uploads finish</Banner>
      <Banner kind="ok">Saved.</Banner>
      <Panel>
        <Tabs
          tabs={[
            { id: "today", label: "Today" },
            { id: "schedule", label: "Schedule" },
            { id: "shows", label: "Shows" },
            { id: "settings", label: "Settings" },
          ]}
          active={tab}
          onChange={setTab}
        />
        <EmptyState>No episodes yet. Add some to get started.</EmptyState>
        <div className="ui-gallery-line">
          <ProgressBar value={0} />
          <ProgressBar value={0.4} />
          <ProgressBar value={1} />
        </div>
      </Panel>
      {confirming && (
        <ConfirmDialog
          title="Delete channel"
          body="Delete channel 3 Nickelodeon? Its files stay on the drive."
          confirm="Delete"
          danger
          onConfirm={() => setConfirming(false)}
          onCancel={() => setConfirming(false)}
        />
      )}
    </div>
  );
}
