# grok-policyd

Policy and multi-agent supervisor **TCB** for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos).

| | |
|--|--|
| **Meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Language** | `schema/policy.capnp` |
| **Product API** | `grok_policyd_handle_capnp()` (in-process FFI) |
| **C helpers** | `include/grok-policyd/supervisor.h` |

## Cap'n language, FFI call path

**Everyone talks Cap'n.** Callers (sessiond, agent, shell) compose `PolicyEnvelope`
bodies and call **into this library** — they do **not** dial a policyd socket.

| Plane | How |
|-------|-----|
| Seat control | Cap'n + nng → **sessiond** (`session.capnp`) |
| Policy / multi-agent TCB | Cap'n body → **`grok_policyd_handle_capnp`** (this package, linked) |

There is **no** `grok-policyd serve` / `policyd.sock` product path. nng stays on
the seat bus only.

```c
#include <grok-policyd/supervisor.h>

grok_supervisor_t *sup = NULL;
uint8_t *req = /* Cap'n PolicyEnvelope request */;
size_t req_len = /* ... */;
uint8_t *resp = NULL;
size_t resp_len = 0;

grok_supervisor_open(&sup, state_dir, runtime_dir);
if (grok_policyd_handle_capnp(sup, req, req_len, &resp, &resp_len) == 0) {
    /* resp is Cap'n PolicyEnvelope response; free(resp) */
}
grok_supervisor_close(sup);
```

Ops on the Cap'n surface: `status`, `check`, `admit`, `agentStatus`
(see `schema/policy.capnp`).

String `grok_policy_check` remains for CLI/tests.

## Build

```bash
pixi install --locked
pixi run test
pixi run coverage   # optional: Meson -Db_coverage → coverage-out/
```

## Layout

```text
schema/policy.capnp          Cap'n SoT
include/grok-policyd/        Public C ABI (includes handle_capnp)
src/capnp_api.c              Cap'n dispatch → TCB
src/policy.c supervisor.c …  TCB
tests/                       cmocka (incl. Cap'n body round-trips)
```

## License

Apache-2.0. See `LICENSE` and `third_party/NOTICE`.
