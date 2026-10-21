import open from "open";
import { buildApp } from "./app.js";
import { ensureSchedBin } from "./sched.js";

const HOST = "127.0.0.1";
const PORT = 4747;

async function main(): Promise<void> {
  const schedBin = await ensureSchedBin();
  const app = buildApp({ schedBin, logger: true, port: PORT });
  await app.listen({ host: HOST, port: PORT });
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
  if (process.env.CTW_NO_OPEN !== "1") await open(`http://${HOST}:${PORT}/`);
}

main().catch((err: unknown) => {
  console.error(err instanceof Error ? err.message : err);
  process.exit(1);
});
