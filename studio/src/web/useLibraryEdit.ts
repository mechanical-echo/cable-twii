import { useState } from "react";
import { type EditResult, editLibrary, getConfig } from "./api";
import type { EditAction } from "../server/libraryEdit.js";
import { type Failure, toFailure } from "./failure";

export const WII_NOTE = "A Wii playing from the server picks this up when its guide opens, at midnight, or with Reload channels.";
const BREAKS = "This change would break a schedule.";

const FLASH_MS = 2000;
let flash: string | null = null;

export interface LibraryEdit {
  run: (action: EditAction, opts?: { confirm?: (dry: EditResult) => Promise<boolean>; done?: string }) => Promise<EditResult | null>;
  busy: boolean;
  failure: Failure | null;
  notice: string | null;
}

export function useLibraryEdit(server: boolean): LibraryEdit {
  const [busy, setBusy] = useState(false);
  const [failure, setFailure] = useState<Failure | null>(null);
  const [notice, setNotice] = useState<string | null>(() => {
    const kept = flash;
    flash = null;
    return kept;
  });

  async function run(action: EditAction, opts: { confirm?: (dry: EditResult) => Promise<boolean>; done?: string } = {}): Promise<EditResult | null> {
    setBusy(true);
    setFailure(null);
    setNotice(null);
    try {
      const { mtimeMs } = await getConfig();
      if (opts.confirm !== undefined) {
        const dry = await editLibrary({ mtimeMs, dryRun: true, action });
        if (dry.issues.length > 0) {
          setFailure({ message: BREAKS, issues: dry.issues, conflict: false });
          return null;
        }
        if (!(await opts.confirm(dry))) return null;
      }
      const result = await editLibrary({ mtimeMs, action });
      const message = server ? `${opts.done ?? "Saved."} ${WII_NOTE}` : (opts.done ?? "Saved.");
      flash = message;
      setNotice(message);
      window.setTimeout(() => {
        if (flash === message) flash = null;
      }, FLASH_MS);
      return result;
    } catch (err) {
      const found = toFailure(err);
      setFailure({ ...found, conflict: found.conflict && found.message.startsWith("config.toml") });
      return null;
    } finally {
      setBusy(false);
    }
  }

  return { run, busy, failure, notice };
}
