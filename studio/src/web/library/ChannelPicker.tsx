import { NEW_CHANNEL } from "../channels";

interface Props {
  choices: string[];
  value: string;
  newName: string;
  all?: boolean;
  onChange: (value: string) => void;
  onNewName: (name: string) => void;
}

export function ChannelPicker({ choices, value, newName, all = false, onChange, onNewName }: Props) {
  return (
    <>
      <select value={value} onChange={(e) => onChange(e.target.value)}>
        {all && <option value="">All channels</option>}
        {choices.map((c) => (
          <option key={c} value={c}>
            {c}
          </option>
        ))}
        <option value={NEW_CHANNEL}>New channel…</option>
      </select>
      {value === NEW_CHANNEL && <input placeholder="New channel name" value={newName} onChange={(e) => onNewName(e.target.value)} />}
    </>
  );
}
