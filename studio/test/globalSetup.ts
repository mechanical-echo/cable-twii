import { ensureSchedBin } from "../src/server/sched.js";

export default async function setup(): Promise<void> {
  await ensureSchedBin();
}
