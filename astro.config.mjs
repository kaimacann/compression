// @ts-check
import { defineConfig } from "astro/config";
import mdx from "@astrojs/mdx";
import react from "@astrojs/react";
import { DOCS_VITE_OPTIMIZE_DEPS } from "@terra-symposium/docs/astro";

export default defineConfig({
  integrations: [mdx(), react()],
  vite: { optimizeDeps: { include: [...DOCS_VITE_OPTIMIZE_DEPS] } },
});
