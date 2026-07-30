# grok-policyd

Policy and multi-agent supervisor (security-critical core) for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos).

| | |
|--|--|
| **Meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Language** | `schema/policy.capnp` (+ `schema/util.capnp`) — SoT also in [grokos-schema](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-schema) |
| **Product API** | `grok_policyd_handle_capnp()` (in-process FFI) |
| **C helpers** | `include/grok-policyd/supervisor.h` |

## Cap'n FFI

Callers (sessiond, agent, shell) compose `PolicyEnvelope` bodies and call
**`grok_policyd_handle_capnp`** on a **linked** `libgrok_policyd` (shared or
static). Cap'n pure-C runtime is the **c-capnproto** package (`libcapnp_c`).

Do **not** copy this tree into consumer `vendor/policyd/` and recompile with
`cc` — that drifts from the TCB and breaks the packaging story. Consumers use
`pkg-config grok-policyd` or monorepo `GROK_POLICYD_DIR=$PWD/build` after
`pixi run lib`.

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
