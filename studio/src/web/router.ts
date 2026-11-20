export type ChannelTab = "today" | "schedule" | "shows" | "settings";

export type Route =
  | { page: "episodes" }
  | { page: "add"; to: "episodes" | "ads"; channel: number | null }
  | { page: "channels" }
  | { page: "channel"; number: number; tab: ChannelTab }
  | { page: "folder"; folder: string }
  | { page: "ads" }
  | { page: "server" }
  | { page: "library" }
  | { page: "schedule" }
  | { page: "preview" }
  | { page: "ui" };

export type TopTab = "episodes" | "channels" | "ads" | "server";

const CHANNEL_TABS: readonly ChannelTab[] = ["today", "schedule", "shows", "settings"];
const PLAIN_PAGES = ["channels", "ads", "server", "library", "schedule", "preview", "ui"] as const;
const EPISODES: Route = { page: "episodes" };

function channelNumber(text: string | null | undefined): number | null {
  if (text === null || text === undefined || !/^\d+$/.test(text)) return null;
  const n = Number(text);
  return n >= 1 ? n : null;
}

export function parseRoute(hash: string): Route {
  const raw = hash.startsWith("#") ? hash.slice(1) : hash;
  const [pathPart, query = ""] = raw.split("?", 2);
  const parts = pathPart.split("/").filter((p) => p !== "");
  if (parts.length === 0) return EPISODES;
  const [head, ...rest] = parts;
  if (head === "add" && rest.length === 0) {
    const params = new URLSearchParams(query);
    return { page: "add", to: params.get("to") === "ads" ? "ads" : "episodes", channel: channelNumber(params.get("channel")) };
  }
  if (head === "channels" && rest.length > 0 && rest.length <= 2) {
    const number = channelNumber(rest[0]);
    const tab = (rest[1] ?? "today") as ChannelTab;
    if (number === null || !CHANNEL_TABS.includes(tab)) return EPISODES;
    return { page: "channel", number, tab };
  }
  if (head === "folders" && rest.length === 1) {
    try {
      return { page: "folder", folder: decodeURIComponent(rest[0]) };
    } catch {
      return EPISODES;
    }
  }
  if (rest.length === 0) {
    const plain = PLAIN_PAGES.find((p) => p === head);
    if (plain !== undefined) return { page: plain };
  }
  return EPISODES;
}

export function routeHash(route: Route): string {
  switch (route.page) {
    case "episodes":
      return "#/";
    case "add": {
      const params = new URLSearchParams();
      if (route.to === "ads") params.set("to", "ads");
      if (route.channel !== null) params.set("channel", String(route.channel));
      const query = params.toString();
      return query === "" ? "#/add" : `#/add?${query}`;
    }
    case "channel":
      return `#/channels/${route.number}/${route.tab}`;
    case "folder":
      return `#/folders/${encodeURIComponent(route.folder)}`;
    default:
      return `#/${route.page}`;
  }
}

export function topTab(route: Route): TopTab | null {
  switch (route.page) {
    case "episodes":
      return "episodes";
    case "add":
      return route.to === "ads" ? "ads" : "episodes";
    case "channels":
    case "channel":
    case "folder":
      return "channels";
    case "ads":
      return "ads";
    case "server":
      return "server";
    default:
      return null;
  }
}

export function leaveAllowed(dirty: boolean, confirm: () => boolean): boolean {
  return !dirty || confirm();
}
