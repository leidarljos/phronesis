# grok-policyd

Policy and multi-agent supervisor **TCB** for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos).

| | |
|--|--|
| **Meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Language** | `schema/policy.capnp` |
| **Product API** | `grok_policyd_handle_capnp()` (in-process FFI) |
| **C helpers** | `include/grok-policyd/supervisor.h` |

## Cap'n FFI

Callers (sessiond, agent, shell) compose `PolicyEnvelope` bodies and call
**`grok_policyd_handle_capnp`** on a linked `libgrok_policyd`. Cap'n pure-C
runtime is the **c-capnproto** package (`libcapnp_c`).

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

Ops: `status`, `check`, `admit`, `agentStatus` (see `schema/policy.capnp`).

String `grok_policy_check` remains for CLI/tests.

## Policy check table (`grok_policy_eval`)

Tools **default deny**. High-risk **actions** (`delete`, `network`, `sudo`,
`secret_export`, …) evaluate to **prompt** before any allow. Agent workspace
comes from `grok_supervisor_start` (empty workspace → path allows fail closed).

| tool | action | path | decision |
|------|--------|------|----------|
| `seat` | `publish_run` / `read_run` / `list_runs` / `list_events` | (unused) | allow |
| `model` | `start` | (unused / optional bind later) | allow |
| `fs` | `read` / `write` | clean absolute under workspace | allow |
| `shell` | `exec` | clean absolute **cwd or target root** under workspace | allow |
| `shell` | `exec` | empty, outside workspace, unclean (`..`, `//`, relative) | deny |
| \* | high-risk action name | any | prompt |
| \* | other | any | deny |

`shell`/`exec` path is **not** argv. Callers map a model bash tool to
`tool=shell`, `action=exec`, and an absolute workspace path (meta #88 Track 1).
Per-tool gates inside the agent model loop are Track 2 (agent package).

Lexical workspace rules match `fs` path checks: absolute paths only; reject
empty components, `.`, and `..`. No `realpath` (symlink escape remains open).

## Build

```bash
pixi install --locked
pixi run test
pixi run coverage   # optional gcovr
```

## Layout

```text
schema/policy.capnp          Cap'n SoT
include/grok-policyd/        Public C ABI (includes handle_capnp)
src/capnp_api.c              Cap'n dispatch → TCB
src/policy.c supervisor.c …  TCB
tests/                       cmocka (Cap'n FFI + lifecycle)
```

## License

Apache-2.0. See `LICENSE` and `third_party/NOTICE`.
