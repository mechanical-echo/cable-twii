import type { ReactNode } from "react";
import { type Route, topTab } from "./router";

interface Props {
  server: boolean;
  route: Route;
  onNavigate: (r: Route) => void;
  children: ReactNode;
}

interface TabLink {
  label: string;
  route: Route;
  active: boolean;
}

export function Shell({ server, route, onNavigate, children }: Props) {
  const top = topTab(route);
  const tabs: TabLink[] = [
    { label: "Episodes", route: { page: "episodes" }, active: top === "episodes" },
    { label: "Channels", route: { page: "channels" }, active: top === "channels" },
    { label: "Schedule", route: { page: "schedule" }, active: route.page === "schedule" },
    { label: "Preview", route: { page: "preview" }, active: route.page === "preview" },
    { label: server ? "Server" : "Drive", route: { page: "server" }, active: top === "server" },
  ];
  return (
    <div className="ui-shell">
      <header className="ui-topbar">
        <a
          className="ui-brand"
          href="#/"
          onClick={(e) => {
            e.preventDefault();
            onNavigate({ page: "episodes" });
          }}
        >
          cable-twii Studio
        </a>
        <nav className="ui-nav">
          {tabs.map((tab) => (
            <button key={tab.label} className={tab.active ? "ui-navtab on" : "ui-navtab"} aria-current={tab.active ? "page" : undefined} onClick={() => onNavigate(tab.route)}>
              {tab.label}
            </button>
          ))}
        </nav>
      </header>
      <main className={route.page === "schedule" ? "ui-main wide" : "ui-main"}>{children}</main>
    </div>
  );
}
