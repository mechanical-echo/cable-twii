import { buildApp } from "./app.js";
import { ensureSchedBin } from "./sched.js";
import { initServerLibrary, readServerSettings, serverSettingsPath } from "./serverSettings.js";

const HOST = "0.0.0.0";

async function main(): Promise<void> {
  const settings = await readServerSettings(serverSettingsPath());
  await initServerLibrary(settings);
  const schedBin = await ensureSchedBin();
  const { library, password, imports, name, port } = settings;
  const app = buildApp({ schedBin, logger: true, port, server: { library, password, imports, name } });
  await app.listen({ host: HOST, port });
  let closing = false;
  const shutdown = () => {
    if (closing) return;
    closing = true;
    app.close().then(
      () => process.exit(0),
      (err: unknown) => {
        console.error(err);
        process.exit(1);
      },
    );
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);
  console.log(`cable-twii server "${name}" on port ${port}, library ${library}`);
}

main().catch((err: unknown) => {
  console.error(err instanceof Error ? err.message : err);
  process.exit(1);
});
