# grok-policyd

Policy and multi-agent supervisor trusted computing base for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos). Non-language-model host library and Cap'n peer for agent lifecycle and fail-closed tool checks.

| | |
|--|--|
| **Meta project** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Catalog** | `packages/MANIFEST.yml` in the meta repository |
| **Public C interface** | `include/grok-policyd/supervisor.h` |
| **Package version** | `VERSION` |
| **API generation** | `API_VERSION` |

Claim work on meta issues tagged for **grok-policyd**. Every merge request should link `trace-analysis/grokos#N`. Prefer host unit tests; fail closed on security and policy paths. Signed commits are required (`./scripts/setup-commit-signing.sh` if signing fails; do not disable `commit.gpgsign`).

```bash
git clone git@ssh.nova.teachx.ai:trace-analysis/grokos-packages/grok-policyd.git
```

## Two planes

| Plane | Package | Role |
|-------|---------|------|
| **Seat** session goals | [grokos-session](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-session) | Cap'n Goal / CancelGoal and run board (`session.capnp`, nng Cap'n body) |
| **Multi-agent TCB** | **this package** | Agent start / stop / status / log and default-deny tool policy; Cap'n **peer** on `policy.capnp` |

Embedders use the **C library** (`libgrok_policyd`). Cross-process use is `grok-policyd serve` over nng req/rep: message body is Cap'n `schema/policy.capnp` (no stream frame). That peer **wraps** the library; it is **not** the seat Cap'n server.

## What it does

- **start / status / stop / log** for agent processes (process-group leader)
- **stop**: process-group `SIGTERM`→`SIGKILL`; Linux best-effort `cgroup.kill` when available
- **action log** JSON Lines under state
- **policy check**: tools default **deny**; high-risk → **prompt**; workspace `read`/`write` may **allow** (lexical allowlist)
- **Seat / model skeleton**: `tool=seat` visibility ops and `model`/`start` admit ALLOW (same-uid peercred is the real gate); unknown admit kinds fail closed
- **cmocka** host tests (single suite binary)

## Public surface

```c
#include <grok-policyd/supervisor.h>
```

| Counter | Source | Purpose |
|---------|--------|---------|
| Package semver | `VERSION` | Release number |
| SONAME major | package major | `libgrok_policyd.so.0` |
| API generation | `API_VERSION` | C link-compat (`GROK_POLICYD_API_VERSION`) |

Edit `VERSION` / `API_VERSION`, run `./scripts/sync-version.sh`, gate with `./scripts/check-version.sh`.

```c
grok_supervisor_t *sup = NULL;
char *argv[] = { "sleep", "60", NULL };
grok_supervisor_open(&sup, state_dir, runtime_dir);
grok_supervisor_start(sup, "agent-a", "agent", workspace, argv);
grok_supervisor_stop(sup, "agent-a");
grok_supervisor_close(sup);
```

## Cap'n peer

```bash
grok-policyd serve --socket "$XDG_RUNTIME_DIR/grokos/policyd.sock"
```

| | |
|--|--|
| Transport | nng req/rep, `ipc://`, mode 0600, `NNG_OPT_PEER_UID` same-uid |
| Body | Cap'n multi-segment `PolicyEnvelope` (`schema/policy.capnp`) via c-capnproto |
| TCB link split | `libgrok_policyd` has no nng / systemd / libcap; those stay on the CLI |

sessiond dials this peer for seat authorize when product path is Cap'n-required. Agents dial for `model`/`start`.

## Build and test

```bash
pixi install --locked
pixi run test    # meson + cmocka + smokes
pixi run ci      # env-info + build
```

Host tests: **cmocka** only (`tests/test_*.c` → one `supervisor_test` binary).

## Layout

```text
include/grok-policyd/supervisor.h   public C ABI
schema/policy.capnp                 Cap'n peer SoT
src/                                TCB + host.c + wire/serve.c
tests/                              cmocka
systemd/grok-policyd.service        user unit (Type=notify)
```

## License

Apache-2.0. See `LICENSE` and third-party notices under `third_party/`.
