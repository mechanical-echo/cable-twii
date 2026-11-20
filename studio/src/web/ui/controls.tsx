import type { ReactNode } from "react";

interface PillProps {
  kind?: "plain" | "primary" | "danger";
  small?: boolean;
  disabled?: boolean;
  onClick?: () => void;
  type?: "button" | "submit";
  children: ReactNode;
}

export function PillButton({ kind = "plain", small = false, disabled = false, onClick, type = "button", children }: PillProps) {
  const classes = ["ui-pill"];
  if (kind !== "plain") classes.push(kind);
  if (small) classes.push("small");
  return (
    <button className={classes.join(" ")} type={type} disabled={disabled} onClick={onClick}>
      {children}
    </button>
  );
}

interface SelectProps<T extends string> {
  value: T;
  options: { value: T; label: string }[];
  onChange: (v: T) => void;
  disabled?: boolean;
  label?: string;
}

export function Select<T extends string>({ value, options, onChange, disabled = false, label }: SelectProps<T>) {
  const select = (
    <select className="ui-select" value={value} disabled={disabled} aria-label={label} onChange={(e) => onChange(e.target.value as T)}>
      {options.map((o) => (
        <option key={o.value} value={o.value}>
          {o.label}
        </option>
      ))}
    </select>
  );
  if (label === undefined) return select;
  return (
    <label className="ui-field">
      <span className="ui-label">{label}</span>
      {select}
    </label>
  );
}

interface TextFieldProps {
  value: string;
  onChange: (v: string) => void;
  label?: string;
  placeholder?: string;
  disabled?: boolean;
  width?: "short" | "full";
  type?: "text" | "password" | "date";
}

export function TextField({ value, onChange, label, placeholder, disabled = false, width, type = "text" }: TextFieldProps) {
  const input = (
    <input
      className={width === undefined ? "ui-input" : `ui-input ${width}`}
      type={type}
      value={value}
      placeholder={placeholder}
      disabled={disabled}
      aria-label={label}
      onChange={(e) => onChange(e.target.value)}
    />
  );
  if (label === undefined) return input;
  return (
    <label className={width === "full" ? "ui-field full" : "ui-field"}>
      <span className="ui-label">{label}</span>
      {input}
    </label>
  );
}

interface CheckboxProps {
  checked: boolean;
  onChange: (v: boolean) => void;
  disabled?: boolean;
  label?: string;
  ariaLabel?: string;
}

export function Checkbox({ checked, onChange, disabled = false, label, ariaLabel }: CheckboxProps) {
  return (
    <label className={disabled ? "ui-check disabled" : "ui-check"}>
      <input type="checkbox" checked={checked} disabled={disabled} aria-label={ariaLabel ?? label} onChange={(e) => onChange(e.target.checked)} />
      <span className="ui-box" />
      {label !== undefined && <span>{label}</span>}
    </label>
  );
}
