interface Props<T extends string> {
  tabs: { id: T; label: string }[];
  active: T;
  onChange: (id: T) => void;
}

export function Tabs<T extends string>({ tabs, active, onChange }: Props<T>) {
  return (
    <div className="ui-tabs" role="tablist">
      {tabs.map((tab) => (
        <button key={tab.id} className={tab.id === active ? "ui-tab on" : "ui-tab"} role="tab" aria-selected={tab.id === active} onClick={() => onChange(tab.id)}>
          {tab.label}
        </button>
      ))}
    </div>
  );
}
