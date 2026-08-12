#!/usr/bin/env node
/**
 * Copy playground/dist-wasm → playground/astro/public/wasm
 * and playground/fixtures → playground/astro/public/fixtures
 * for static serving under Astro (dev + build).
 *
 * Does not invoke emcc. Missing dist-wasm is a soft warning so `astro
 * build` can still produce HTML; the island loads WASM only after click.
 */
import {
  cpSync,
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const playground = join(here, "..");
const distWasm = join(playground, "dist-wasm");
const fixturesSrc = join(playground, "fixtures");
const astroPublic = join(playground, "astro", "public");
const wasmDest = join(astroPublic, "wasm");
const fixturesDest = join(astroPublic, "fixtures");
const catalogDest = join(playground, "astro", "src", "lib", "fixture-catalog.json");

const NEEDED = [
  "phronesis-playground.js",
  "phronesis-playground.wasm",
  "phronesis-playground.data",
];

function copyTree(src, dest) {
  if (existsSync(dest)) rmSync(dest, { recursive: true, force: true });
  mkdirSync(dirname(dest), { recursive: true });
  cpSync(src, dest, { recursive: true });
}

mkdirSync(astroPublic, { recursive: true });

let wasmOk = true;
if (!existsSync(distWasm)) {
  console.warn(
    `warn: missing ${distWasm} — build on rg.terra: bash playground/wasm/build.sh`,
  );
  wasmOk = false;
} else {
  for (const f of NEEDED) {
    if (!existsSync(join(distWasm, f))) {
      console.warn(`warn: missing ${join(distWasm, f)}`);
      wasmOk = false;
    }
  }
  if (wasmOk) {
    copyTree(distWasm, wasmDest);
    console.log(`copied wasm → ${relative(playground, wasmDest)}`);
  }
}

const DOMAINS = ["shell", "path", "seat", "risk"];
const catalog = [];

if (existsSync(fixturesSrc)) {
  copyTree(fixturesSrc, fixturesDest);
  console.log(`copied fixtures → ${relative(playground, fixturesDest)}`);
  for (const domain of DOMAINS) {
    const dir = join(fixturesSrc, domain);
    if (!existsSync(dir)) continue;
    for (const name of readdirSync(dir).sort()) {
      if (!name.endsWith(".json")) continue;
      const path = join(dir, name);
      try {
        const raw = JSON.parse(readFileSync(path, "utf8"));
        catalog.push({
          id: raw.id ?? name.replace(/\.json$/, ""),
          domain,
          method: raw.method,
          description: raw.description ?? "",
          file: `${domain}/${name}`,
          expect: raw.expect ?? null,
        });
      } catch (e) {
        console.warn(`warn: skip fixture ${path}: ${e}`);
      }
    }
  }
} else {
  console.warn(`warn: missing fixtures at ${fixturesSrc}`);
}

mkdirSync(dirname(catalogDest), { recursive: true });
writeFileSync(catalogDest, JSON.stringify(catalog, null, 2) + "\n");
console.log(`wrote catalog (${catalog.length}) → ${relative(playground, catalogDest)}`);

if (!wasmOk) {
  console.warn(
    "WASM assets incomplete; /play will show Load evaluator until dist-wasm is present.",
  );
}
process.exit(0);
