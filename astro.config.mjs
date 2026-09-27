// @ts-check
import { defineConfig } from "astro/config";
import starlight from "@astrojs/starlight";

// https://astro.build/config
export default defineConfig({
  integrations: [
    starlight({
      title: "Compression Project Assessment",
      description:
        "Readable Markdown documentation for Assessment Task 3: Group Project.",
      customCss: ["./src/styles/custom.css"],
      sidebar: [
        {
          label: "Contents",
          items: [{ autogenerate: { directory: "." } }],
        },
      ],
    }),
  ],
});
