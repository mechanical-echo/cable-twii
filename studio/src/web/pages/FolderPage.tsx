import { ShowsTab } from "../channel/ShowsTab";
import { PageHead } from "../ui/layout";

export function FolderPage({ folder }: { folder: string }) {
  return (
    <section>
      <PageHead title={folder} sub="Not on a channel" />
      <ShowsTab folder={folder} />
    </section>
  );
}
