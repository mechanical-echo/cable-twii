import type { Issue } from "./api";
import { CONFLICT_MESSAGE, type Failure, describeIssue } from "./failure";

interface Props {
  failure: Failure | null;
  onReload: () => void;
  where?: (issue: Issue) => string;
}

export function FailureBanner({ failure, onReload, where }: Props) {
  if (failure === null) return null;
  return (
    <div className="banner error">
      {failure.conflict ? (
        <div className="row">
          <span className="grow">{CONFLICT_MESSAGE}</span>
          <button onClick={onReload}>Reload</button>
        </div>
      ) : (
        failure.message
      )}
      {failure.issues.length > 0 && (
        <ul>
          {failure.issues.map((issue, i) => {
            const place = where?.(issue) ?? "";
            return <li key={i}>{place !== "" ? `${place}: ${describeIssue(issue)}` : describeIssue(issue)}</li>;
          })}
        </ul>
      )}
    </div>
  );
}
