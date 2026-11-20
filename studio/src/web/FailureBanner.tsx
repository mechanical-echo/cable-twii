import type { Issue } from "./api";
import { CONFLICT_MESSAGE, type Failure, describeIssue } from "./failure";
import { PillButton } from "./ui/controls";
import { Banner } from "./ui/layout";

interface Props {
  failure: Failure | null;
  onReload: () => void;
  where?: (issue: Issue) => string;
}

export function FailureBanner({ failure, onReload, where }: Props) {
  if (failure === null) return null;
  return (
    <Banner kind="error">
      <div className="grow">
        {failure.conflict ? CONFLICT_MESSAGE : failure.message}
        {failure.issues.length > 0 && (
          <ul>
            {failure.issues.map((issue, i) => {
              const place = where?.(issue) ?? "";
              return <li key={i}>{place !== "" ? `${place}: ${describeIssue(issue)}` : describeIssue(issue)}</li>;
            })}
          </ul>
        )}
      </div>
      {failure.conflict && (
        <PillButton small onClick={onReload}>
          Reload
        </PillButton>
      )}
    </Banner>
  );
}
