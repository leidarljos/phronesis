# phronesis playground (WASM)

Browser/Node playground for Cap'n policy checks against the product TCB sources.
**Not** part of the product default CI / meson path.

**UI system:** [`DESIGN.md`](DESIGN.md) — dark forensic-tool chrome synthesized from
[awesome-design-md](https://github.com/VoltAgent/awesome-design-md) patterns
(Linear canvas + accent, Raycast semantic status, Warp mono, Mintlify encyclopedia density).
CSS tokens live in `astro/src/styles/global.css`.


## Multi-pack (product parity)

The WASM evaluator uses the same colon-list / `packs.d` multi-pack host as
product `PHRONESIS_JANET_PACK` and Cap'n `reloadShellPack`:

| MEMFS path | Role |
|------------|------|
| `/policy/shell.janet` | Product entry (shell-check + audio-check) |
| `/policy/lib/*.janet` | Pure helpers for the product entry |
| `/policy/packs.d/*.janet` | Optional extra packs (seeded canary) |

Default load: `/policy/shell.janet:/policy/packs.d`. Host composition is
**deny > prompt > allow**. Author mode reloads the full colon list after MEMFS edits.

## Quick start (reviewers — no emcc)

Prebuilt WASM is committed under `playground/dist-wasm/`. You only need **Node 20+**
(and optionally `pixi` for the product suite).

```bash
# from this repo root
git fetch origin
git checkout feat/policyd-playground   # or the MR source branch

# 1) Node smoke (no browser)
node playground/scripts/wasm-smoke.mjs
# expect: OK checkShell allow code=20

node playground/scripts/wasm-smoke.mjs --expect-trace
# expect: OK checkShell deny code=24 … spans+shortCircuit

node playground/scripts/parity-node.mjs
# expect: parity OK 13 fixtures

# 2) Web UI (dev server)
cd playground/astro
npm ci
PUBLIC_BASE=/ npm run dev -- --host 127.0.0.1 --port 4321
# open http://127.0.0.1:4321/play
#   → Load evaluator (or ⌘L / Ctrl+L)
#   → leave default curl|sh argv (or pick fixture curl_sh_deny)
#   → Evaluate (⌘↵) → DENY / PolicyReason 24 / token highlights / TRACE steps
#   → press ? for the full shortcuts list (chips under the toolbar always show the main chords)
```

Or from repo root: `pixi run playground-dev` (runs `npm ci` + `astro dev` under
`playground/astro`; set `PUBLIC_BASE=/` if asset paths look wrong).

### JavaScript quality (Astro / TypeScript)

There is **no** product-meson gate for the playground UI. Local and continuous
integration use **Biome** (lint + format) and **`tsc --noEmit`** on
`playground/astro` + `playground/scripts`:

```bash
# from this repo root
cd playground/astro && npm ci && npm run check
# or: pixi run playground-check
```

| Command | What |
|---------|------|
| `npm run lint` | Biome check (TS/TSX + Node scripts) |
| `npm run lint:fix` | Biome write fixes |
| `npm run typecheck` | `tsc --noEmit` (strict) |
| `npm run check` | lint + typecheck |

Job **`playground:check`** runs the same on playground path changes. Astro
`.astro` pages are out of Biome’s TypeScript scope (frontmatter false positives);
`tsc` covers the island components.

### Static site (Pages-shaped, optional)

```bash
# from this repo root
PUBLIC_BASE=/phronesis/ bash playground/scripts/build-site.sh
python3 -m http.server 8765 --directory playground/astro/dist
# if base is /phronesis/, serve a parent dir that contains phronesis/ as
# the dist tree, or use PUBLIC_BASE=/ for a root-relative static preview:
PUBLIC_BASE=/ bash playground/scripts/build-site.sh
python3 -m http.server 8765 --directory playground/astro/dist
# open http://127.0.0.1:8765/play/
```

### Product suite (optional, heavy — use a builder host)

```bash
pixi install --locked
pixi run test
# product cmocka must stay green; playground options default off
```

### Rebuild WASM (only if you change TCB / packs)

Requires **emcc** (typically on `rg.terra`, not a laptop). See
[Build path](#build-path-v1-direct-emcc) below, then commit updated
`playground/dist-wasm/` if you intend Pages/CI to use the new binary.

## Build path (v1): direct `emcc`

Meson cross-file exists at `wasm/meson-cross-emscripten.ini` for later use.
v1 builds with a single script that compiles product C + deps with Emscripten:

```text
playground/wasm/build.sh
  → gen Cap'n C (host capnpc-c)
  → seed MEMFS (policy pack, /ws/ok.py, agent slot, /pd-state, /pd-runtime)
  → emcc link → playground/dist-wasm/policyd-playground.{js,wasm,data}
```

### Why direct emcc

- Product `meson.build` expects host `c-capnproto`, cmocka, threads, shared `.so`.
- Playground needs wasm32, MEMFS preload, KEEPALIVE exports, no cmocka.
- One `emcc` line is less fragile than dual-target meson for this smoke.

### Prerequisites (build host: **rg.terra**)

1. Product pixi env (host `capnp` / `capnpc-c` / compilers for fixture export):
   ```bash
   pixi install --locked
   eval "$(pixi shell-hook)"
   ```
2. Emscripten (`emcc` on `PATH`), e.g.:
   ```bash
   # emsdk
   git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
   ~/emsdk/emsdk install latest
   ~/emsdk/emsdk activate latest
   source ~/emsdk/emsdk_env.sh

   # or conda-forge via a side env
   pixi global install emscripten   # if available for platform
   ```
3. Node (smoke only): system node or pixi `nodejs`.
4. Network once: build clones `HaoZeke/c-capnproto` into `playground/wasm/deps/`
   (gitignored). Override with `CAPN_C_SRC=/path/to/c-capnproto`.

### Build

```bash
bash playground/wasm/build.sh
# → playground/dist-wasm/policyd-playground.js
# → playground/dist-wasm/policyd-playground.wasm
# → playground/dist-wasm/policyd-playground.data  (MEMFS preload)
```

### Smoke

```bash
node playground/scripts/wasm-smoke.mjs
# stdout: OK checkShell allow code=20

node playground/scripts/wasm-smoke.mjs --expect-trace
# stdout: OK checkShell deny code=24 trace_events=N spans+shortCircuit
```

Smoke flow (default):

1. Dynamic `import()` of the MODULARIZE ES module.
2. `pd_supervisor_open("/pd-state", "/pd-runtime")` (runtime is **not** under `/tmp`).
3. Agent workspace `/ws` from seeded slot file (no `fork`/`start`).
4. Cap'n `ShellCheck` fixture: `["uv","run","--script","ok.py"]`, cwd `/ws`.
5. Expect `Decision.allow` (1) and `PolicyReason.shellExecAllow` (20).

`--expect-trace` uses `shell_check_curl_sh.bin` (`curl` + `sh` tokens), expects
deny code 24, and asserts `pd_take_trace_json` yields a non-empty TraceEvent
array with argv spans and `shortCircuit` on the deny path.

### Fixture catalog + parity

JSON fixtures under `playground/fixtures/{shell,path,seat,risk}/` mirror
cmocka cases (`test_shell_pack`, path/seat/risk policy). Schema:
`playground/fixtures/schema.json`. Cap'n request bytes are built by
`playground/scripts/capnp-encode.mjs` (optional golden `bin` per fixture).

```bash
# After dist-wasm exists (build on rg.terra, rsync back if needed):
node playground/scripts/parity-node.mjs
# stdout: parity OK N fixtures

pixi run playground-parity   # same, no emcc
```

Parity fails the process if any fixture's `decision`+`code` drifts. Shared
GitLab runners do not ship emsdk: jobs `playground:wasm` (manual,
`allow_failure`) and `playground:parity` (`allow_failure` until artifacts
exist) document the terra build path. Product `pixi run test` is unchanged.

### MEMFS layout (preload root `/`)

| Path | Purpose |
|------|---------|
| `/policy/shell.janet` + `/policy/lib/*.janet` | Product shell pack |
| `/ws/ok.py` | PEP 723 script for path probes |
| `/pd-state/…` | Supervisor state (not `/tmp`) |
| `/pd-runtime/agents/<id>.slot` | Agent with workspace `/ws` |
| `PHRONESIS_JANET_PACK` | Default `/policy/shell.janet` |

### KEEPALIVE API (`wasm/embind_api.c`)

| Symbol | Role |
|--------|------|
| `pd_supervisor_open` | Open TCB; returns opaque handle |
| `pd_supervisor_close` | Close |
| `pd_check_shell` | Cap'n in → malloc'd Cap'n `PolicyDecision` (clears TRACE ring) |
| `pd_check_path` | Cap'n `PathCheck` → `PolicyDecision` |
| `pd_check_seat` | Cap'n `SeatCheck` → `PolicyDecision` |
| `pd_check_risk` | Cap'n `RiskCheck` → `PolicyDecision` |
| `pd_reload_shell_pack` | Cap'n `ReloadShellPack` → `PolicyDecision` |
| `pd_reload_pack_path` | Absolute path string → 0 / -1 / -2 (no Cap'n) |
| `pd_read_decision` | Decode decision + code for JS |
| `pd_clear_trace` | Clear TRACE ring (no-op without `PHRONESIS_TRACE`) |
| `pd_take_trace_json` | Malloc JSON array of TraceEvents; clears ring; free with `pd_free` |
| `pd_free` | Free malloc'd out buffers |

Playground `build.sh` always sets `-DPHRONESIS_TRACE=1`. Product meson never
does; product `.so` has no TRACE dynamic exports.

### Meson options (product tree)

```text
-Dplayground=false          # default; product CI stays host .so + cmocka
-Dplayground_trace=false    # reserved for extra playground logging
```

These do **not** change the default build. Direct emcc does not require them.

### Product regression

```bash
pixi install --locked
pixi run test   # cmocka green; no playground flags
```

## Astro site (Probe / Author UI)

Three-pane island under `playground/astro/` (Astro 5 + Preact):

| Route | Role |
|-------|------|
| `/` | Landing |
| `/play` | Probe (method form + decision + TRACE) and Author (MEMFS pack editor) |
| `/fixtures` | Fixture catalog |
| `/fixtures/<id>` | Fixture detail + link into `/play?fixture=` |
| `/encyclopedia` | PolicyReason index (all ordinals) |
| `/encyclopedia/<code>` | Per-code page (codegen + product notes) |

### Lazy WASM

`/play` renders HTML without loading WASM. Click **Load evaluator** to
dynamic-import `public/wasm/policyd-playground.js` (copied from
`playground/dist-wasm` by `copy-wasm-assets.mjs` on `predev` / `prebuild`).

### Base path (GitLab Pages)

Default local `base` is `/phronesis/`. CI derives `PUBLIC_BASE` from
`CI_PAGES_URL` so nested-group path Pages and unique-domain Pages both work.
Override:

```bash
PUBLIC_BASE=/ npm run build          # site at domain root
PUBLIC_BASE=/phronesis/ npm run build
```

### GitLab Pages (members-only)

The playground is **not** a public demo. Access level must be **Only project
members** (API: `pages_access_level=private`).

| Setting | Value |
|---------|--------|
| Pages access | **Only project members** |
| Public custom domain | **Do not** attach one for the playground |
| CI job | `pages` (stage `deploy`) → artifact `public/` on **every** default-branch pipeline |
| Live URL | GitLab `CI_PAGES_URL` + `/play/` (also in the `pages` job log as `PAGES_URL=…`) |

**UI:** Settings → General → Visibility → Pages → *Only project members*.  
**Deploy UI:** Deploy → Pages (shows the site URL after the first green `pages` job).

**Checklist / API assert:**

```bash
bash playground/scripts/assert-pages-private.sh          # print checklist
bash playground/scripts/assert-pages-private.sh --check  # glab / token API
```

#### CI publish path

Job `pages` needs `playground/dist-wasm` (for a working `/play` evaluator),
then runs `playground/scripts/build-site.sh` and copies `playground/astro/dist`
to `public/` for GitLab Pages.

Shared runners **lack emsdk**. Honest options:

1. Run manual job `playground:wasm` on an **emsdk-capable** image so artifacts
   flow into `pages` via `needs` (optional until that image exists).
2. Build on **rg.terra** (`pixi run playground-wasm`), ensure
   `playground/dist-wasm/` is present for the pipeline (artifact from wasm job
   or tree), then re-run `pages` (manual rule on default branch).

`pages` **fails closed** if `dist-wasm` is incomplete — it will not publish an
HTML-only shell that pretends the evaluator works. `playground:parity` remains
soft (`allow_failure`) until emsdk runners are default.

`needs`: optional `playground:wasm` (artifacts) + optional `playground:parity`.
Rules: **manual** on default branch (after dist-wasm exists); optional path-filtered
entry when playground files change. Script still fails closed without WASM.

### Build site (no emcc)

```bash
# Required for a full evaluator site: dist-wasm (from rg.terra build)
ls playground/dist-wasm/policyd-playground.{js,wasm,data}

cd playground/astro
npm ci
npm run build    # → playground/astro/dist/

# or from repo root:
pixi run playground-build
# / bash playground/scripts/build-site.sh
```

Dev server:

```bash
cd playground/astro && npm run dev
# open /play → Load evaluator → fixture curl_sh_deny → DENY / 24 / trace
```

### Share URL

Hash payload `#v1.<base64url(JSON)>` carries mode + method + request fields.
**Share URL** warns when argv looks secret (glpat-/ghp_/basic-auth/…).

### Author mode

Edit MEMFS files under `/policy/` (entry `shell.janet` + `lib/*.janet`), then
**Write + reload pack** (`pd_reload_pack_path` / `pd_reload_shell_pack`).
Next Probe `checkShell` uses the reloaded pack.
