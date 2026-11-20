import { describe, expect, it } from "vitest";
import { leaveAllowed, parentsOf, parseRoute, routeHash, topTab, type Route } from "../src/web/router.js";

describe("parseRoute", () => {
  it("parses every route", () => {
    expect(parseRoute("#/")).toEqual({ page: "episodes" });
    expect(parseRoute("#/add")).toEqual({ page: "add", to: "episodes", channel: null });
    expect(parseRoute("#/add?channel=2")).toEqual({ page: "add", to: "episodes", channel: 2 });
    expect(parseRoute("#/add?to=ads")).toEqual({ page: "add", to: "ads", channel: null });
    expect(parseRoute("#/channels")).toEqual({ page: "channels" });
    expect(parseRoute("#/channels/3")).toEqual({ page: "channel", number: 3, tab: "today" });
    expect(parseRoute("#/channels/3/today")).toEqual({ page: "channel", number: 3, tab: "today" });
    expect(parseRoute("#/channels/3/schedule")).toEqual({ page: "channel", number: 3, tab: "schedule" });
    expect(parseRoute("#/channels/3/shows")).toEqual({ page: "channel", number: 3, tab: "shows" });
    expect(parseRoute("#/channels/3/settings")).toEqual({ page: "channel", number: 3, tab: "settings" });
    expect(parseRoute("#/folders/mtv-00s")).toEqual({ page: "folder", folder: "mtv-00s" });
    expect(parseRoute("#/folders/Old%20Channel")).toEqual({ page: "folder", folder: "Old Channel" });
    expect(parseRoute("#/ads")).toEqual({ page: "ads" });
    expect(parseRoute("#/server")).toEqual({ page: "server" });
  });

  it("unknown hash is episodes", () => {
    expect(parseRoute("#/nope")).toEqual({ page: "episodes" });
    expect(parseRoute("#/library")).toEqual({ page: "episodes" });
    expect(parseRoute("#/preview")).toEqual({ page: "episodes" });
    expect(parseRoute("#/schedule")).toEqual({ page: "episodes" });
    expect(parseRoute("#/ui")).toEqual({ page: "episodes" });
    expect(parseRoute("")).toEqual({ page: "episodes" });
    expect(parseRoute("#/channels/x")).toEqual({ page: "episodes" });
    expect(parseRoute("#/channels/0")).toEqual({ page: "episodes" });
    expect(parseRoute("#/channels/3/nope")).toEqual({ page: "episodes" });
    expect(parseRoute("#/folders/")).toEqual({ page: "episodes" });
    expect(parseRoute("#/folders/%E0%A4%A")).toEqual({ page: "episodes" });
  });

  it("ignores a bad channel in add", () => {
    expect(parseRoute("#/add?channel=x")).toEqual({ page: "add", to: "episodes", channel: null });
  });
});

describe("routeHash", () => {
  it("round trips", () => {
    const routes: Route[] = [
      { page: "episodes" },
      { page: "add", to: "episodes", channel: null },
      { page: "add", to: "episodes", channel: 4 },
      { page: "add", to: "ads", channel: null },
      { page: "add", to: "ads", channel: 2 },
      { page: "channels" },
      { page: "channel", number: 1, tab: "today" },
      { page: "channel", number: 12, tab: "settings" },
      { page: "folder", folder: "mtv-00s" },
      { page: "folder", folder: "Tom & Jerry Old" },
      { page: "ads" },
      { page: "server" },
    ];
    for (const route of routes) expect(parseRoute(routeHash(route))).toEqual(route);
  });

  it("writes the plain hashes", () => {
    expect(routeHash({ page: "episodes" })).toBe("#/");
    expect(routeHash({ page: "add", to: "ads", channel: null })).toBe("#/add?to=ads");
    expect(routeHash({ page: "channel", number: 3, tab: "shows" })).toBe("#/channels/3/shows");
  });
});

describe("topTab", () => {
  it("top tab", () => {
    expect(topTab({ page: "add", to: "ads", channel: null })).toBe("ads");
    expect(topTab({ page: "add", to: "episodes", channel: 2 })).toBe("channels");
    expect(topTab({ page: "add", to: "episodes", channel: null })).toBe("episodes");
    expect(topTab({ page: "channel", number: 1, tab: "today" })).toBe("channels");
    expect(topTab({ page: "folder", folder: "x" })).toBe("channels");
    expect(topTab({ page: "server" })).toBe("server");
  });
});

describe("leaveAllowed", () => {
  it("leaving a dirty page asks and can stay", () => {
    expect(leaveAllowed(true, () => false)).toBe(false);
    expect(leaveAllowed(true, () => true)).toBe(true);
    expect(
      leaveAllowed(false, () => {
        throw new Error("asked");
      }),
    ).toBe(true);
  });
});

describe("parentsOf", () => {
  const names = (n: number) => (n === 3 ? "CN" : null);

  it("top pages have no parents", () => {
    for (const route of [{ page: "episodes" }, { page: "channels" }, { page: "ads" }, { page: "server" }] as Route[]) expect(parentsOf(route, names)).toEqual([]);
  });

  it("channel and folder pages sit under Channels", () => {
    expect(parentsOf({ page: "channel", number: 3, tab: "shows" }, names)).toEqual([{ label: "Channels", route: { page: "channels" } }]);
    expect(parentsOf({ page: "folder", folder: "Old" }, names)).toEqual([{ label: "Channels", route: { page: "channels" } }]);
  });

  it("add pages lead back where they came from", () => {
    expect(parentsOf({ page: "add", to: "episodes", channel: null }, names)).toEqual([{ label: "Episodes", route: { page: "episodes" } }]);
    expect(parentsOf({ page: "add", to: "ads", channel: null }, names)).toEqual([{ label: "Ads", route: { page: "ads" } }]);
    expect(parentsOf({ page: "add", to: "episodes", channel: 3 }, names)).toEqual([
      { label: "Channels", route: { page: "channels" } },
      { label: "CN", route: { page: "channel", number: 3, tab: "today" } },
    ]);
    expect(parentsOf({ page: "add", to: "episodes", channel: 7 }, names)[1]).toEqual({ label: "Channel 7", route: { page: "channel", number: 7, tab: "today" } });
  });
});
