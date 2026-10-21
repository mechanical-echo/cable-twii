import { describe, expect, it } from "vitest";
import {
  applyDelete,
  applyEdit,
  draftFromChannel,
  effectiveFolder,
  folderSuggestions,
  iconPath,
  newDraft,
  nextFreeNumber,
  setMode,
  sortChannels,
  toChannel,
  validateDraft
} from "../src/web/channels.js";
import type { Channel, Config } from "../src/server/types.js";

const cn: Channel = { number: 1, name: "Cartoon Network", category: "Kids", ads: true, mode: "timed", slot: [{ at: "06:00", items: [{ type: "ads", count: 2 }] }] };
const mtv: Channel = { number: 2, name: "MTV", category: "Music", folder: "mtv-00s", ads: false, mode: "looping", loop: [{ type: "ads", count: 3 }] };
const config: Config = { version: 1, seed: 7, channel: [mtv, cn] };

describe("sortChannels", () => {
  it("orders by number without mutating", () => {
    const input = [mtv, cn];
    expect(sortChannels(input).map((c) => c.number)).toEqual([1, 2]);
    expect(input[0]).toBe(mtv);
  });
});

describe("folder and icon defaults", () => {
  it("defaults the folder to the name", () => {
    expect(effectiveFolder(cn)).toBe("Cartoon Network");
    expect(effectiveFolder(mtv)).toBe("mtv-00s");
  });

  it("derives the default icon path unless set", () => {
    expect(iconPath(mtv)).toBe("mtv-00s/channel.png");
    expect(iconPath({ ...cn, icon: "x/y.png" })).toBe("x/y.png");
  });
});

describe("newDraft", () => {
  it("takes the first free number and seeds a valid timed channel", () => {
    const draft = newDraft([cn, mtv]);
    expect(draft.number).toBe("3");
    expect(draft.mode).toBe("timed");
    expect(draft.slot).toEqual([{ at: "00:00", items: [] }]);
    expect(draft.loop).toEqual([{ type: "ads", count: 1 }]);
  });

  it("fills gaps", () => {
    expect(nextFreeNumber([mtv])).toBe(1);
  });
});

describe("setMode", () => {
  it("keeps the other list in memory and seeds it when empty", () => {
    const draft = { ...draftFromChannel(cn), loop: [] };
    const looping = setMode(draft, "looping");
    expect(looping.loop).toEqual([{ type: "ads", count: 1 }]);
    expect(looping.slot).toEqual(cn.slot);
    expect(setMode(looping, "timed").slot).toEqual(cn.slot);
  });
});

describe("toChannel", () => {
  it("saves only the active list and omits the default folder", () => {
    const draft = setMode(draftFromChannel(cn), "looping");
    const channel = toChannel(draft);
    expect(channel.loop).toEqual([{ type: "ads", count: 1 }]);
    expect(channel.slot).toBeUndefined();
    expect(channel.folder).toBeUndefined();
    expect(channel.icon).toBeUndefined();
  });

  it("keeps a custom folder and trims fields", () => {
    const draft = { ...draftFromChannel(mtv), name: " MTV ", category: " Music ", number: " 2 " };
    expect(toChannel(draft)).toEqual(mtv);
  });

  it("drops a folder equal to the name", () => {
    const draft = { ...draftFromChannel(mtv), folder: "MTV" };
    expect(toChannel(draft).folder).toBeUndefined();
  });

  it("uses the name when the folder is blank", () => {
    const draft = { ...draftFromChannel(cn), folder: "  " };
    expect(toChannel(draft).folder).toBeUndefined();
  });
});

describe("validateDraft", () => {
  it("accepts a valid draft", () => {
    expect(validateDraft(draftFromChannel(cn), config.channel, 1)).toEqual([]);
  });

  it("rejects numbers out of range or fractional", () => {
    for (const number of ["0", "1000", "1.5", "", "abc"]) {
      const errors = validateDraft({ ...newDraft([]), name: "A", category: "B", number }, [], null);
      expect(errors[0]).toMatch(/Number must be/);
    }
  });

  it("rejects a duplicate number, but not the channel's own", () => {
    const dup = { ...draftFromChannel(cn), number: "2" };
    expect(validateDraft(dup, config.channel, 1)).toEqual(["Channel number 2 is already used."]);
    expect(validateDraft(draftFromChannel(cn), config.channel, 1)).toEqual([]);
    expect(validateDraft({ ...newDraft([]), name: "A", category: "B", number: "1" }, config.channel, null)).toEqual([
      "Channel number 1 is already used."
    ]);
  });

  it("requires name and category and a plain folder", () => {
    const errors = validateDraft({ ...newDraft([]), folder: "a/b" }, [], null);
    expect(errors).toEqual(["Name is required.", "Category is required.", "Folder must be a single folder name."]);
  });
});

describe("applyEdit and applyDelete", () => {
  it("adds a channel in order", () => {
    const draft = { ...newDraft(config.channel), name: "Nick", category: "Kids" };
    const next = applyEdit(config, null, draft);
    expect(next.channel.map((c) => c.number)).toEqual([1, 2, 3]);
    expect(next.seed).toBe(7);
    expect(config.channel).toHaveLength(2);
  });

  it("replaces on edit, including a renumber", () => {
    const draft = { ...draftFromChannel(cn), number: "9", name: "CN" };
    const next = applyEdit(config, 1, draft);
    expect(next.channel.map((c) => [c.number, c.name])).toEqual([[2, "MTV"], [9, "CN"]]);
  });

  it("deletes by number", () => {
    expect(applyDelete(config, 1).channel).toEqual([mtv]);
  });
});

describe("folderSuggestions", () => {
  it("collects unique non-empty channel folders", () => {
    const library = {
      show: [{ channel: "Cartoon Network" }, { channel: "Cartoon Network" }, { channel: "" }],
      ad: [{ channel: "" }, { channel: "mtv-00s" }]
    };
    expect(folderSuggestions(library as never)).toEqual(["Cartoon Network", "mtv-00s"]);
  });
});

describe("iconPath for drafts", () => {
  it("uses the trimmed folder, falling back to the name", () => {
    expect(iconPath(draftFromChannel(mtv))).toBe("mtv-00s/channel.png");
    expect(iconPath({ ...draftFromChannel(cn), folder: "  " })).toBe("Cartoon Network/channel.png");
    expect(iconPath({ ...draftFromChannel(cn), name: " CN ", folder: " x " })).toBe("x/channel.png");
  });
});
