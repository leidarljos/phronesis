**Agents/contributors:** [AGENTS.md](./AGENTS.md) · [CONTRIBUTING.md](./CONTRIBUTING.md). Issues: [meta](https://nova.teachx.ai/trace-analysis/grokos/-/issues).

# grok-policyd

Policy / multi-agent supervisor TCB for GrokOS (non-LLM). Host library + CLI for agent lifecycle and fail-closed tool checks.

| | |
|--|--|
| **Parent meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Catalog** | `packages/MANIFEST.yml` in meta |
| **Stable C ABI** | `include/grok-policyd/supervisor.h` (package version in `VERSION`, API gen in `API_VERSION`) |

## Two engines (do not confuse them)

| Plane | Package | Role |
|-------|---------|------|
| **Seat** Cap'n Goal / CancelGoal for the human session | [grokos-session](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-session) (`grokos-proc` / `InProcessSupervisor`) | Seat process tree for goals; **already live** |
| **Multi-agent TCB** lifecycle + policy | **this package** (`grok_supervisor_*`) | Agent start/stop/status/log + default-deny tools |

**Host FFI** is the stable C ABI in this package (static or shared `libgrok_policyd`). **Session ↔ policyd Cap'n peer wire** is a separate track (meta #70); it will wrap this library, not replace the header.

### Why not cbindgen?

[rgpot](https://github.com/OmniPotentRPC/rgpot), featomic, and metatensor implement the core in **Rust** and emit C headers with **cbindgen**. This package is pure **C**: the header **is** the source of truth. The same product surface still applies — versioned library, Doxygen on the public API, Sphinx + breathe reference — without a fake cbindgen step. Language bindings (Rust) use hand `extern "C"` or bindgen *against* this header (see `grokos-shell` `product/src/policyd.rs`).

## What this package does (now)

- **start / status / stop / log** for agent processes (process-group leader)
- **stop** = process-group SIGTERM→SIGKILL; on Linux, **best-effort `cgroup.kill`** when a writable cgroup v2 child can be created, then process-group as safety net
- **action log** JSONL under state (`log/actions.jsonl`)
- **policy check**: tools **default deny**; high-risk actions → **prompt**; `read`/`write` under the agent workspace root may **allow** (**lexical** allowlist: absolute paths only, rejects `..` components; **not** realpath — symlink escape still open)
- **Installable C library**: `libgrok_policyd.a` / `.so`, pkg-config, version queries (`grok_policyd_version_string`, `grok_policyd_api_version`)
- **Docs**: Doxygen + Sphinx/breathe (`pixi run -e docs …`)
- **Dev env**: `pixi.toml` + `pixi.lock` (compilers, cmocka, docs)
- Host unit tests via **cmocka** (`make test`; `pkg-config cmocka`; Alpine: `cmocka-dev`); CI runs the same

## What this package does **not** do yet

- No UDS daemon / Cap'n wire between sessiond and policyd (session Cap'n Goal plane is already shipped; missing piece is the **policyd peer**, not seat Cap'n)
- No multi-UID agent identities or systemd unit templates (meta #29)
- No guaranteed cgroup on every host (macOS and locked cgroup hierarchies fall back to process-group; children that `setpgid` away can escape until a real delegated cgroup is required)
- No path **canonicalization** (symlink-based escape past workspace root) — intentional stub limit
- No fake model / capability store (other packages / tickets)
- No full confirm UX (decision is `prompt`; human channel not implemented here)
- High-risk actions are an exact-match string table (stub); a structured tool/action catalog is follow-up (#25 / S3)

## Build

**Entry point is pixi** (same pattern as `grokos-session` / `grokos-shell`).
`Makefile` is the compile recipe backend; do not invent host `apk`/`apt` toolchains for dogfood.

```bash
pixi install --locked
pixi run test                 # lib + CLI + cmocka suites
pixi run lib                  # static + shared lib + pkg-config
pixi run example              # examples/c/minimal.c
pixi run build                # lib + test + example
pixi run ci                   # env-info + build (CI gate)
pixi run install              # PREFIX default /usr/local (override with make)
```

Host tests use **cmocka** from the pixi env (`pkg-config cmocka`).

Version single-source: edit `VERSION` / `API_VERSION`, run `./scripts/sync-version.sh`, gate with `./scripts/check-version.sh` (also `make lib`). SONAME follows **package major**; embedders key on `GROK_POLICYD_API_VERSION` (see docs architecture).

```bash
STATE=$(mktemp -d -p "${XDG_CACHE_HOME:-$HOME/.cache}")
RUN=$(mktemp -d -p "${XDG_CACHE_HOME:-$HOME/.cache}")
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" start agent-a -- sleep 60
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" status agent-a
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" check agent-a shell exec
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" check agent-a fs read /nope
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" stop agent-a
```

Paths: `GROKOS_STATE_DIR`, `GROKOS_RUNTIME_DIR`, `GROKOS_ACTION_LOG`, else XDG host defaults. Runtime root `/tmp` is rejected.

### Documentation

```bash
pixi install --locked -e docs
pixi run -e docs doxygen      # HTML + XML (breathe)
pixi run -e docs docs         # doxygen + Sphinx HTML
```

Architecture notes: `docs/source/architecture.rst`, `docs/orgmode/architecture.org`.
Host tests (cmocka): `docs/source/testing.rst`.

### Embedding (C)

```bash
cc $(pkg-config --cflags grok-policyd) myapp.c \
   $(pkg-config --libs grok-policyd) -o myapp
```

Public header only: `#include <grok-policyd/supervisor.h>`.

## Layout

```
include/grok-policyd/   public headers (stable C ABI)
src/                    library + CLI
tests/                  host tests (cmocka)
examples/c/             minimal C consumer
docs/                   Doxygen + Sphinx/breathe
packaging/              pkg-config template
build/                  outputs (gitignored)
```

## Clone

```bash
git clone git@ssh.nova.teachx.ai:trace-analysis/grokos-packages/grok-policyd.git
```
