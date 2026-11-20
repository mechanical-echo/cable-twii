import type { Config } from "../api";
import type { Route } from "../router";
import { Chip } from "../ui/status";

interface Props {
  numbers: number[];
  config: Config;
  onNavigate: (route: Route) => void;
  off?: boolean;
}

export function AirsOnChips({ numbers, config, onNavigate, off = false }: Props) {
  if (off) return <Chip kind="warn">Off air</Chip>;
  if (numbers.length === 0) return <Chip>Not airing</Chip>;
  return (
    <span className="ui-airs-on">
      {numbers.map((n) => (
        <button
          key={n}
          className="ui-chip ok link"
          onClick={(e) => {
            e.stopPropagation();
            onNavigate({ page: "channel", number: n, tab: "schedule" });
          }}
        >
          {config.channel.find((c) => c.number === n)?.name ?? `Channel ${n}`}
        </button>
      ))}
    </span>
  );
}
