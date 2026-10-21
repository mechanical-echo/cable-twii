import path from "node:path";
import { fileURLToPath } from "node:url";
import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

const studio = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  root: path.join(studio, "src", "web"),
  plugins: [react()],
  build: {
    outDir: path.join(studio, "dist", "web"),
    emptyOutDir: true,
  },
  server: {
    proxy: {
      "/api": "http://127.0.0.1:4747",
      "/media": "http://127.0.0.1:4747",
    },
  },
});
