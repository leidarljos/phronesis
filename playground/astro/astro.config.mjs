// @ts-check

import path from "node:path";
import { fileURLToPath } from "node:url";
import preact from "@astrojs/preact";
import { defineConfig } from "astro/config";

const root = path.dirname(fileURLToPath(import.meta.url));

/**
 * GitLab Pages for a package often lives at
 *   https://<group>.pages.../grok-policyd/
 * Override with PUBLIC_BASE (must end with / when non-root).
 * Local dev / plain static: PUBLIC_BASE=/ or leave unset for relative-friendly.
 */
const base = process.env.PUBLIC_BASE ?? "/grok-policyd/";

// https://astro.build/config
export default defineConfig({
  base,
  integrations: [preact({ compat: true })],
  vite: {
    resolve: {
      alias: {
        "@lib": path.join(root, "src/lib"),
        "@components": path.join(root, "src/components"),
      },
    },
    server: {
      fs: {
        allow: [root, path.join(root, "..")],
      },
    },
  },
});
