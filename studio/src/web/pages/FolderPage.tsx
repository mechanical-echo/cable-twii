import { ShowsTab } from "../channel/ShowsTab";
import { type Route, parentsOf } from "../router";
import { Crumbs, PageHead } from "../ui/layout";

export function FolderPage({ folder, onNavigate }: { folder: string; onNavigate: (route: Route) => void }) {
  return (
    <section>
      <Crumbs items={parentsOf({ page: "folder", folder }, () => null)} onNavigate={onNavigate} />
      <PageHead title={folder} sub="Not on a channel" />
      <ShowsTab folder={folder} />
    </section>
  );
}
