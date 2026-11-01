import { existsSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const repositoryRoot = dirname(fileURLToPath(import.meta.url));
const docsEntry = fileURLToPath(import.meta.resolve("@terra-symposium/docs"));
let docsPackageRoot = dirname(docsEntry);
while (!existsSync(resolve(docsPackageRoot, "site/astro.config.mjs"))) {
  const parent = dirname(docsPackageRoot);
  if (parent === docsPackageRoot)
    throw new Error("Could not locate the Terra Docs site's Astro configuration.");
  docsPackageRoot = parent;
}
const docsConfigPath = resolve(docsPackageRoot, "site/astro.config.mjs");
const { default: docsConfig } = await import(
  /* @vite-ignore */ pathToFileURL(docsConfigPath).href
);

export default {
  ...docsConfig,
  root: repositoryRoot,
  srcDir: resolve(repositoryRoot, "src"),
  publicDir: resolve(repositoryRoot, "public"),
};
