import type { ReactNode } from "react";
import { type Crumb, type Route, routeHash } from "../router";

export function Panel({ title, actions, children }: { title?: string; actions?: ReactNode; children: ReactNode }) {
  return (
    <section className="ui-panel">
      {(title !== undefined || actions !== undefined) && (
        <div className="ui-panel-head">
          <h2 className="ui-panel-title">{title}</h2>
          {actions}
        </div>
      )}
      {children}
    </section>
  );
}

interface RowProps {
  selected?: boolean;
  dimmed?: boolean;
  onClick?: () => void;
  children: ReactNode;
}

export function ListRow({ selected = false, dimmed = false, onClick, children }: RowProps) {
  const classes = ["ui-row"];
  if (selected) classes.push("selected");
  if (dimmed) classes.push("dimmed");
  if (onClick === undefined) return <div className={classes.join(" ")}>{children}</div>;
  classes.push("clickable");
  return (
    <div
      className={classes.join(" ")}
      role="button"
      tabIndex={0}
      onClick={onClick}
      onKeyDown={(e) => {
        if (e.target !== e.currentTarget || (e.key !== "Enter" && e.key !== " ")) return;
        e.preventDefault();
        onClick();
      }}
    >
      {children}
    </div>
  );
}

export function Banner({ kind, children }: { kind: "error" | "warn" | "ok"; children: ReactNode }) {
  return <div className={`ui-banner ${kind}`}>{children}</div>;
}

export function EmptyState({ children }: { children: ReactNode }) {
  return <div className="ui-empty">{children}</div>;
}

export function PageHead({ title, sub, actions }: { title: string; sub?: ReactNode; actions?: ReactNode }) {
  return (
    <header className="ui-pagehead">
      <div className="ui-pagehead-text">
        <h1>{title}</h1>
        {sub !== undefined && <div className="ui-pagehead-sub">{sub}</div>}
      </div>
      {actions !== undefined && <div className="ui-pagehead-actions">{actions}</div>}
    </header>
  );
}

export function Crumbs({ items, onNavigate }: { items: Crumb[]; onNavigate: (route: Route) => void }) {
  if (items.length === 0) return null;
  return (
    <nav className="ui-crumbs-bar" aria-label="You are here">
      <span aria-hidden="true">‹</span>
      {items.map((item, i) => (
        <span key={routeHash(item.route)} className="ui-crumb-item">
          {i > 0 && <span aria-hidden="true">›</span>}
          <a
            href={routeHash(item.route)}
            onClick={(e) => {
              e.preventDefault();
              onNavigate(item.route);
            }}
          >
            {item.label}
          </a>
        </span>
      ))}
    </nav>
  );
}

export function rowLink(base: string, open: (() => void) | undefined) {
  if (open === undefined) return { className: base };
  return {
    className: `${base} link`,
    role: "button",
    tabIndex: 0,
    onClick: open,
    onKeyDown: (e: React.KeyboardEvent) => {
      if (e.key === "Enter" || e.key === " ") {
        e.preventDefault();
        open();
      }
    },
  };
}
