# grok-policyd

Policy and multi-agent supervisor **library** (TCB) for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos). Fail-closed tool checks and agent lifecycle for the seat stack.

| | |
|--|--|
| **Meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Public C API** | `include/grok-policyd/supervisor.h` |
| **Version** | `VERSION` / `API_VERSION` |

Claim meta issues tagged **grok-policyd**. Link `trace-analysis/grokos#N` on every MR. Signed commits required.

## Library only

Seat Cap'n stays on **sessiond** (nng + `session.capnp`). This package is the
**in-process** policy/supervisor TCB: link `libgrok_policyd` and call
`grok_policy_check` / `grok_supervisor_*`. No serve process, no `policyd.sock`.

Rules today are simple (default deny, high-risk → prompt, workspace path
allowlist, seat/model skeleton allows). Callers own Cap'n composition and any
higher-level policy state; they pass tool/action/path strings into this ABI.

## Build

```bash
pixi install --locked
pixi run test
```

## Layout

```text
include/grok-policyd/supervisor.h
src/     TCB + small CLI (start/stop/check)
tests/   cmocka
```

## License

Apache-2.0. See `LICENSE` and `third_party/`.
