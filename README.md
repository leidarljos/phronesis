# grok-policyd

Policy and multi-agent supervisor trusted computing base for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos). Non-language-model host library and command-line tool for agent lifecycle and fail-closed tool checks.

| | |
|--|--|
| **Meta project** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Catalog** | `packages/MANIFEST.yml` in the meta repository |
| **Public C interface** | `include/grok-policyd/supervisor.h` |
| **Package version** | `VERSION` |
| **Application programming interface generation** | `API_VERSION` |

Claim work on meta issues tagged for **grok-policyd**. Every merge request here should link `trace-analysis/grokos#N`. Prefer host unit tests; fail closed on security and policy paths. Signed commits are required — run `./scripts/setup-commit-signing.sh` if signing fails; do not disable `commit.gpgsign`.

```bash
git clone git@ssh.nova.teachx.ai:trace-analysis/grokos-packages/grok-policyd.git
```

## Two planes

GrokOS has two related but separate surfaces. Do not mix them up.

| Plane | Package | Role |
|-------|---------|------|
| **Seat** session goals | [grokos-session](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-session) (`grokos-proc` / `InProcessSupervisor`) | Cap'n Proto Goal / CancelGoal for the human session process tree — already live |
| **Multi-agent trusted computing base** | **this package** (`grok_supervisor_*`) | Agent start / stop / status / log and default-deny tool policy |

The stable embedder surface here is the **C library** (static or shared `libgrok_policyd`). Cross-process use is a Cap'n **peer** (meta issue #70): own Unix domain socket, frame magic **GKPP**, schema `schema/policy.capnp`, started with `grok-policyd serve`. That peer **wraps** this library; it does not replace the header and is **not** the seat Cap'n server (seat bus stays **GKSP** / `session.capnp` in grokos-session).

The implementation is pure C, so the public header is hand-maintained source of truth (not a cbindgen export). Language bindings (for example Rust in `grokos-shell`) use hand-written `extern "C"` or bindgen against this header.

## What it does

- **start / status / stop / log** for agent processes (process-group leader)
- **stop** sends process-group `SIGTERM` then `SIGKILL`; on Linux, best-effort `cgroup.kill` when a writable cgroup version 2 child can be created, with process-group as the safety net
- **action log** as JSON Lines under state (`log/actions.jsonl`)
- **policy check**: tools **default deny**; high-risk actions return **prompt**; `read` / `write` under the agent workspace root may **allow** via a **lexical** allowlist (absolute paths only, rejects `..` components — not realpath, so symlink escape past the workspace is still open)
- Installable library: `libgrok_policyd.a` / `.so`, pkg-config, version queries (`grok_policyd_version_string`, `grok_policyd_api_version`)
- Host unit tests with **cmocka**

Current limits worth knowing when embedding: multi-UID agent identities and systemd unit templates are not shipped; macOS and locked cgroup hierarchies fall back to process-group; high-risk actions are an exact-match string table until a structured catalog lands; Cap'n codec on the peer path is a minimal hand codec until a full capnp-c cutover.

### Cap'n peer and seat spine

The seat Cap'n Goal plane and run board live in **grokos-session** (server). This package is the multi-agent trusted computing base and Cap'n **peer**.

| Item | Role |
|------|------|
| **Host C surface** | Embedders (shell/agent today) link `supervisor.h` for lifecycle and `policy_check` |
| **Cap'n peer** (`serve`, GKPP, `policy.capnp`) | Cross-process wire; sessiond is Cap'n **client** when `policyd.sock` is present |
| **Seat gate (skeleton)** | Same-uid peercred + socket mode is the real isolation gate; `tool=seat` and model/`start` admit stay broad ALLOW for seat dogfood; unknown admit kinds fail closed |

Shell and agent stay Cap'n clients of **sessiond** only. They do not open a Cap'n listener on policyd.

## Public surface

Include only:

```c
#include <grok-policyd/supervisor.h>
```

Opaque handle `grok_supervisor_t` may change layout freely. Public structs, enums, fixed buffer sizes, and function signatures stay stable within a `GROK_POLICYD_API_VERSION` generation. Additive functions do not require an application programming interface bump.

Three version counters (do not conflate them):

| Counter | Source | Purpose |
|---------|--------|---------|
| Package semantic version | `VERSION` | Human / release number (`0.1.0`) |
| Shared object name major | package major | ELF link name `libgrok_policyd.so.0` while major is 0 |
| Application programming interface generation | `API_VERSION` | Link-compat for the C surface; embedders key on `GROK_POLICYD_API_VERSION` |

Edit `VERSION` / `API_VERSION`, run `./scripts/sync-version.sh`, gate with `./scripts/check-version.sh` (also run from `make lib`).

Minimal open / start / stop:

```c
#include "grok-policyd/supervisor.h"

grok_supervisor_t *sup = NULL;
char *argv[] = { "sleep", "60", NULL };

if (grok_supervisor_open(&sup, state_dir, runtime_dir) != GROK_OK)
    return 1;
grok_supervisor_start(sup, "agent-a", "agent", workspace, argv);
grok_supervisor_stop(sup, "agent-a");
grok_supervisor_close(sup);
```

Version check at load time:

```c
if (grok_policyd_api_version() < GROK_POLICYD_API_VERSION) {
    /* headers newer than linked library */
}
```

## Build and test

**Entry point is pixi** (same pattern as `grokos-session` / `grokos-shell`). The `Makefile` is the compile recipe backend; do not invent ad-hoc host package-manager toolchains for day-to-day work.

```bash
pixi install --locked
pixi run test                 # library + command-line tool + cmocka suites
pixi run lib                  # static + shared library + pkg-config
pixi run example              # examples/c/minimal.c
pixi run build                # lib + test + example
pixi run ci                   # env-info + build (continuous integration gate)
pixi run install              # PREFIX default /usr/local
```

Host tests need **cmocka** from the pixi environment (`pkg-config cmocka`). Suites live under `tests/test_*.c` (paths, lifecycle, kill tree, action log, persistence, policy, version).

### Command-line smoke

Runtime root under `/tmp` is rejected. Prefer cache or `/var/tmp`.

```bash
STATE=$(mktemp -d -p "${XDG_CACHE_HOME:-$HOME/.cache}")
RUN=$(mktemp -d -p "${XDG_CACHE_HOME:-$HOME/.cache}")
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" start agent-a -- sleep 60
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" status agent-a
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" check agent-a shell exec
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" check agent-a fs read /nope
./build/grok-policyd --state-dir "$STATE" --runtime-dir "$RUN" stop agent-a
```

Paths resolve from `GROKOS_STATE_DIR`, `GROKOS_RUNTIME_DIR`, `GROKOS_ACTION_LOG`, else XDG host defaults.

### Link from C

```bash
cc $(pkg-config --cflags grok-policyd) myapp.c \
   $(pkg-config --libs grok-policyd) -o myapp
```

Or vendor `src/*.c` into a static archive (as `grokos-shell` does). pkg-config template: `packaging/grok-policyd.pc.in` (expanded by `make pc` / `make install`).

## Layout

```
include/grok-policyd/   public headers (stable C interface)
src/                    library + command-line tool
tests/                  host tests (cmocka)
examples/c/             minimal C consumer
packaging/              pkg-config template
scripts/                version sync, signing setup
build/                  outputs (gitignored)
```

License: Apache-2.0 (`LICENSE`).
