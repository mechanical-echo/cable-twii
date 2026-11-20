import { useEffect, useState } from "react";
import { getLibrary } from "../api";
import type { WebLibrary } from "../tree";
import { Banner, EmptyState, ListRow, PageHead } from "../ui/layout";

export function FolderPage({ folder }: { folder: string }) {
  const [library, setLibrary] = useState<WebLibrary | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    getLibrary()
      .then(setLibrary)
      .catch((err: Error) => setError(err.message));
  }, []);

  const shows = library?.show.filter((s) => s.channel === folder) ?? [];
  return (
    <section>
      <PageHead title={folder} sub="Not on a channel" />
      {error !== null && <Banner kind="error">{error}</Banner>}
      {library !== null && shows.length === 0 && <EmptyState>No shows in this folder.</EmptyState>}
      {shows.map((show) => (
        <ListRow key={show.folder}>
          <span className="grow ui-row-name">{show.name}</span>
          <span className="ui-muted">{show.episode.length} episodes</span>
        </ListRow>
      ))}
    </section>
  );
}
