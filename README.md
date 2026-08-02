# grok-policyd

Policy and multi-agent supervisor (security-critical core) for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos).

| | |
|--|--|
| **Meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Language** | `schema/policy.capnp` (+ `schema/util.capnp`) — SoT also in [grokos-schema](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-schema) |
| **Product API** | `grok_policyd_handle_capnp()` (in-process FFI) |
| **C helpers** | `include/grok-policyd/supervisor.h` |

## Cap'n product API (`interface Policyd`)

Schema SoT: `schema/policy.capnp` (from grokos-schema). Product surface is
**methods with typed results**, not free tool/action Text and not C errno for
policy outcomes.

| Method | Params | Results |
|--------|--------|---------|
| `status` | — | `StatusResults` = `PolicydStatus` \| `Err` |
| `check` | `CheckParams` | `CheckResults` = `PolicyDecision` \| `Err` |
| `admit` | `AdmitParams` | `CheckResults` (same) |
| `agentStatus` | `AgentStatusParams` | `AgentStatusResults` = `AgentStatus` \| `Err` |

- **`Decision`** (deny/allow/prompt) lives inside `CheckResults.ok`.
- **`Err`** is protocol/TCB failure — distinct from deny.
- **`CheckParams.body`** is a domain **union**: `seat` \| `model` \| `path` \|
  `shell` \| `risk` (invalid tool×action pairs are unrepresentable).
- **Identity** is `Util.AgentId` / `Util.TraceId` bits only (never Text hex).
- **Shell content** is `ShellOp { cwd, argv : List(Text) }` (spawn argv).

Hosts without Cap'n RPC pack **`CallEnvelope`** and call
`grok_policyd_handle_capnp` (in-process). c-capnproto does not emit interface
stubs; Params/Results structs are the wire shapes.

```c
#include <grok-policyd/supervisor.h>

grok_supervisor_t *sup = NULL;
uint8_t *req = /* Cap'n CallEnvelope: body.check = CheckParams */;
size_t req_len = /* ... */;
uint8_t *resp = NULL;
size_t resp_len = 0;

grok_supervisor_open(&sup, state_dir, runtime_dir);
if (grok_policyd_handle_capnp(sup, req, req_len, &resp, &resp_len) == 0) {
    /* resp: CallEnvelope body.checkResults = CheckResults { ok | err } */
    free(resp);
}
grok_supervisor_close(sup);
```

`grok_policy_check` remains a **CLI string bridge** only (legacy tool/action
text). Product agents speak Cap'n methods.

### CheckBody decision table

| body arm | condition | Decision |
|----------|-----------|----------|
| `seat` | publishRun / readRun / listRuns / listEvents | allow |
| `model` | ModelOp (empty model ok) | allow |
| `path` | read/write, clean abs under workspace | allow |
| `path` | delete | prompt |
| `shell` | cwd under workspace, empty argv | allow |
| `shell` | cwd under workspace, argv Python without `uv`+`run` | **deny** |
| `shell` | argv `uv run` + `.py` without PEP 723 | **deny** |
| `shell` | argv `uv run` + `.py` with PEP 723 `# /// script` | allow |
| `risk` | any set RiskAction | prompt |
| other | — | deny |

Lexical workspace rules: absolute paths only; reject `//`, `.`, `..`. No
`realpath`.

## Build / test / coverage (pixi only)

```bash
# once: https://pixi.sh
pixi install --locked
pixi run env-info
pixi run test                 # meson compile + cmocka (incl. Cap'n FFI)
pixi run coverage             # Meson -Db_coverage + gcovr → coverage-out/
# CI: coverage:pixi job runs the same `pixi run coverage` and requires
# coverage-out/coverage.xml + coverage.txt (no optional path).
```

Do **not** hand-edit `pixi.lock`. Refresh with `pixi lock` on a builder that has network.

### Mutation testing (Mull)

Optional. Needs **Clang** and a matching **Mull** install (`mull-runner-N` +
`mull-ir-frontend-N` from [Mull packages](https://mull.readthedocs.io/en/latest/Installation.html)).
Config: [`mull.yml`](mull.yml) (scopes mutants to `src/policy.c` and
`src/capnp_api.c`).

```bash
pixi install --locked
export PATH="$PWD/.pixi/envs/ci/bin:$PATH"
export PKG_CONFIG_PATH="$PWD/.pixi/envs/ci/lib/pkgconfig"
export LD_LIBRARY_PATH="$PWD/.pixi/envs/ci/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export CC=clang-19   # must be real Clang; not conda gcc from `pixi run`
meson setup build-mull -Dmutation=true -Dbuildtype=debug
meson compile -C build-mull mutation
# report: build-mull/mull-report/
```

Optional: `-Dmull_pass_plugin=/path/to/mull-ir-frontend-19`.

The Meson `mutation` target passes `--allow-surviving` so the report is always
produced without failing the build on known survivors. To gate later: drop that
flag or set Mull’s `--mutation-score-threshold`.

### Continuous integration

Job **`mutation:mull`**: Ubuntu 24.04, system Clang + Mull, pixi for deps only
(`CC=/usr/bin/clang-19`). Path-triggered on policy/Cap'n changes and on
schedules. Prints the IDE survivor list at the end of the job log; full report
under artifact `build-mull/mull-report/`.

## Layout

```text
schema/policy.capnp          Cap'n API (pin from grokos-schema)
schema/util.capnp            Shared vocab (RunState, …); imported by policy
include/grok-policyd/        Public C ABI (includes handle_capnp)
src/capnp_api.c              Cap'n dispatch → TCB
src/policy.c supervisor.c …  TCB
tests/                       cmocka (Cap'n FFI + lifecycle)
scripts/coverage.sh          gcovr report (hard-requires gcovr from pixi)
```

## License

Apache-2.0. See `LICENSE` and `third_party/NOTICE`.

## Deny-all (tests / lockdown)

Set `GROKOS_POLICYD_DENY_ALL=1` (or `true`/`yes`) to force every `policy_check` / Cap'n check to **deny**. Used to prove agent/sessiond fail closed under a hard seat. Unset for normal allowlists.
