const major = Number(process.versions.node.split(".")[0]);

if (major < 22) {
  console.error("cable-twii Studio needs Node 22 — run `nvm use` in the studio folder");
  process.exit(1);
}
