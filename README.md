**Agents/contributors:** [AGENTS.md](./AGENTS.md) · [CONTRIBUTING.md](./CONTRIBUTING.md). Issues: [meta](https://nova.teachx.ai/trace-analysis/grokos/-/issues).

# grok-policyd

Policy / multi-agent supervisor TCB for GrokOS (non-LLM). Host library + CLI for agent lifecycle and fail-closed tool checks.

| | |
|--|--|
| **Parent meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Catalog** | `packages/MANIFEST.yml` in meta |
| **Issue** | https://nova.teachx.ai/trace-analysis/grokos/-/issues/28 |

## Two engines (do not confuse them)

| Plane | Package | Role |
|-------|---------|------|
| **Seat** Cap'n Goal / CancelGoal for the human session | [grokos-session](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-session) (`grokos-proc` / `InProcessSupervisor`) | Seat process tree for goals; **already live** |
| **Multi-agent TCB** lifecycle + policy | **this package** (`grok_supervisor_*`) | Agent start/stop/status/log + default-deny tools |

session ↔ policyd is **Cap'n Proto over UDS only** (no cbindgen / shared-lib into session). This MR is the host C library a future policyd daemon will wrap; Cap'n wire is not implemented here yet.

## What this package does (now)

- **start / status / stop / log** for agent processes (process-group leader)
- **stop** = process-group SIGTERM→SIGKILL; on Linux, **best-effort `cgroup.kill`** when a writable cgroup v2 child can be created, then process-group as safety net
- **action log** JSONL under state (`log/actions.jsonl`)
- **policy check**: tools **default deny**; high-risk actions → **prompt**; `read`/`write` under the agent workspace root may **allow** (**lexical** allowlist: absolute paths only, rejects `..` components; **not** realpath — symlink escape still open)
- Host unit tests (`make test`); CI runs the same

## What this package does **not** do yet

- No UDS daemon / Cap'n wire between sessiond and policyd (session Cap'n Goal plane is already shipped; missing piece is the **policyd peer**, not seat Cap'n)
- No multi-UID agent identities or systemd unit templates (meta #29)
- No guaranteed cgroup on every host (macOS and locked cgroup hierarchies fall back to process-group; children that `setpgid` away can escape until a real delegated cgroup is required)
- No path **canonicalization** (symlink-based escape past workspace root) — intentional stub limit
- No fake model / capability store (other packages / tickets)
- No full confirm UX (decision is `prompt`; human channel not implemented here)
- High-risk actions are an exact-match string table (stub); a structured tool/action catalog is follow-up (#25 / S3)
- Host tests use **cmocka** (`pkg-config cmocka`; Alpine: `cmocka-dev`)

## Build

```bash
# needs cmocka + pkg-config (brew install cmocka / apk add cmocka-dev pkgconf)
make test
make                 # build/grok-policyd
```

```bash
STATE=$(mktemp -d)
RUN=$(mktemp -d)
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" start agent-a -- sleep 60
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" status agent-a
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" check agent-a shell exec
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" check agent-a fs read /nope
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" stop agent-a
```

Paths: `GROKOS_STATE_DIR`, `GROKOS_RUNTIME_DIR`, `GROKOS_ACTION_LOG`, else XDG host defaults. Runtime root `/tmp` is rejected.

## Layout

```
include/grok-policyd/   public headers
src/                    library + CLI
tests/                  host tests
build/                  outputs (gitignored)
packaging/              RPM
```

## Clone

```bash
git clone git@ssh.nova.teachx.ai:trace-analysis/grokos-packages/grok-policyd.git
```
