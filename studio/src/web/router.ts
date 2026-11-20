export type ChannelTab = "today" | "schedule" | "shows" | "settings";

export type Route =
  | { page: "episodes" }
  | { page: "add"; to: "episodes" | "ads"; channel: number | null }
  | { page: "channels" }
  | { page: "channel"; number: number; tab: ChannelTab }
  | { page: "folder"; folder: string }
  | { page: "ads" }
  | { page: "shows" }
  | { page: "show"; channel: string; show: string }
  | { page: "episode"; path: string }
  | { page: "ad"; path: string }
  | { page: "server" };

export type TopTab = "episodes" | "channels" | "shows" | "ads" | "server";

const CHANNEL_TABS: readonly ChannelTab[] = ["today", "schedule", "shows", "settings"];
const PLAIN_PAGES = ["channels", "shows", "ads", "server"] as const;
const EPISODES: Route = { page: "episodes" };

function channelNumber(text: string | null | undefined): number | null {
  if (text === null || text === undefined || !/^\d+$/.test(text)) return null;
  const n = Number(text);
  return n >= 1 ? n : null;
}

function decodeAll(parts: string[]): string[] | null {
  try {
    return parts.map((p) => decodeURIComponent(p));
  } catch {
    return null;
  }
}

function encodePath(path: string): string {
  return path.split("/").map(encodeURIComponent).join("/");
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
  if (head === "shows" && rest.length === 2) {
    const decoded = decodeAll(rest);
    return decoded === null ? EPISODES : { page: "show", channel: decoded[0]!, show: decoded[1]! };
  }
  if ((head === "episode" || head === "ad") && rest.length >= 2) {
    const decoded = decodeAll(rest);
    return decoded === null ? EPISODES : { page: head, path: decoded.join("/") };
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
    case "show":
      return `#/shows/${encodeURIComponent(route.channel)}/${encodeURIComponent(route.show)}`;
    case "episode":
    case "ad":
      return `#/${route.page}/${encodePath(route.path)}`;
    default:
      return `#/${route.page}`;
  }
}

export function topTab(route: Route): TopTab | null {
  switch (route.page) {
    case "episodes":
      return "episodes";
    case "add":
      if (route.to === "ads") return "ads";
      return route.channel === null ? "episodes" : "channels";
    case "channels":
    case "channel":
    case "folder":
      return "channels";
    case "ads":
    case "ad":
      return "ads";
    case "shows":
    case "show":
    case "episode":
      return "shows";
    case "server":
      return "server";
    default:
      return null;
  }
}

export interface Crumb {
  label: string;
  route: Route;
}

const CHANNELS: Crumb = { label: "Channels", route: { page: "channels" } };

const SHOWS: Crumb = { label: "Shows", route: { page: "shows" } };

export function parentsOf(route: Route, channelName: (number: number) => string | null, showName: (ref: { channel: string; show: string }) => string | null = () => null): Crumb[] {
  switch (route.page) {
    case "show":
      return [SHOWS];
    case "episode": {
      const [channel = "", show = ""] = route.path.split("/");
      return [SHOWS, { label: showName({ channel, show }) ?? show, route: { page: "show", channel, show } }];
    }
    case "ad":
      return [{ label: "Ads", route: { page: "ads" } }];
    case "channel":
    case "folder":
      return [CHANNELS];
    case "add":
      if (route.to === "ads") return [{ label: "Ads", route: { page: "ads" } }];
      if (route.channel === null) return [{ label: "Episodes", route: { page: "episodes" } }];
      return [CHANNELS, { label: channelName(route.channel) ?? `Channel ${route.channel}`, route: { page: "channel", number: route.channel, tab: "today" } }];
    default:
      return [];
  }
}

export function leaveAllowed(dirty: boolean, confirm: () => boolean): boolean {
  return !dirty || confirm();
}
