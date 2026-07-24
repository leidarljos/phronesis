**Agents/contributors:** [AGENTS.md](./AGENTS.md) · [CONTRIBUTING.md](./CONTRIBUTING.md). Issues: [meta](https://nova.teachx.ai/trace-analysis/grokos/-/issues).

# grok-policyd

Policy / supervisor TCB for GrokOS (non-LLM). Host library + CLI for agent lifecycle and fail-closed tool checks.

| | |
|--|--|
| **Parent meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Catalog** | `packages/MANIFEST.yml` in meta |
| **Issue** | https://nova.teachx.ai/trace-analysis/grokos/-/issues/28 |

## What this package does (now)

- **start / status / stop / log** for agent processes (process-group leader)
- **stop** = process-group SIGTERM→SIGKILL; on Linux, **best-effort `cgroup.kill`** when a writable cgroup v2 child can be created, then process-group as safety net
- **action log** JSONL under state (`log/actions.jsonl`)
- **policy check**: tools **default deny**; high-risk actions → **prompt**; `read`/`write` under the agent workspace root may **allow**
- Host unit tests (`make test`); CI runs the same

## What this package does **not** do yet

- No UDS daemon / session Cap’n wiring (session still `SupervisorIpcNotImplemented` until a client lands)
- No multi-UID agent identities or systemd unit templates
- No guaranteed cgroup on every host (macOS and locked cgroup hierarchies fall back to process-group; children that `setpgid` away can escape until a real delegated cgroup is required)
- No fake model / capability store (other packages / tickets)
- No full confirm UX (decision is `prompt`; human channel not implemented here)

## Build

```bash
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
