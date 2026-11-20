import { type ReactNode, useState } from "react";
import { ConfirmDialog } from "./ConfirmDialog";

interface Ask {
  title: string;
  body: ReactNode;
  confirm: string;
  danger?: boolean;
}

export function useConfirm(): [ReactNode, (ask: Ask) => Promise<boolean>] {
  const [open, setOpen] = useState<(Ask & { resolve: (ok: boolean) => void }) | null>(null);
  const ask = (next: Ask) => new Promise<boolean>((resolve) => setOpen({ ...next, resolve }));
  const close = (ok: boolean) => {
    open?.resolve(ok);
    setOpen(null);
  };
  const dialog = open === null ? null : <ConfirmDialog title={open.title} body={open.body} confirm={open.confirm} danger={open.danger} onConfirm={() => close(true)} onCancel={() => close(false)} />;
  return [dialog, ask];
}

export function RemovedList({ removed, channelName }: { removed: { channel: number; label: string }[]; channelName: (n: number) => string }) {
  if (removed.length === 0) return null;
  return (
    <>
      <p>These schedule items name it and will be removed:</p>
      <ul>
        {removed.map((r, i) => (
          <li key={i}>
            {channelName(r.channel)}: {r.label}
          </li>
        ))}
      </ul>
    </>
  );
}
