import { useState } from "react";

export function IconImage({ src }: { src: string }) {
  const [failed, setFailed] = useState<string | null>(null);
  return <img className="icon" src={src} alt="" style={{ visibility: failed === src ? "hidden" : "visible" }} onError={() => setFailed(src)} />;
}
