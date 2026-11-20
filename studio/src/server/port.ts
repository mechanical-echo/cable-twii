const DEFAULT_PORT = 4747;

export function drivePort(env: NodeJS.ProcessEnv): number {
  const raw = env.CTW_PORT;
  if (raw === undefined || !/^\d+$/.test(raw)) return DEFAULT_PORT;
  const port = Number(raw);
  return port >= 1 && port <= 65535 ? port : DEFAULT_PORT;
}
