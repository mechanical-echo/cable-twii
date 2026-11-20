import { useState } from "react";
import { PillButton } from "../ui/controls";

interface Props {
  src: string;
  title: string;
  onClose: () => void;
}

export function SourcePreview({ src, title, onClose }: Props) {
  const [failed, setFailed] = useState(false);
  return (
    <div className="overlay" onClick={onClose} onKeyDown={(e) => e.key === "Escape" && onClose()}>
      <div className="ui-dialog ui-preview" role="dialog" aria-modal="true" aria-label={title} onClick={(e) => e.stopPropagation()}>
        <h2>{title}</h2>
        {failed ? <div className="ui-preview-failed">This browser can't preview this file</div> : <video src={src} controls autoPlay onError={() => setFailed(true)} />}
        <div className="ui-dialog-actions">
          <PillButton onClick={onClose}>Close</PillButton>
        </div>
      </div>
    </div>
  );
}
