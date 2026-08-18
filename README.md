# grok-policyd

Policy and multi-agent supervisor (security-critical core) for [GrokOS](https://nova.teachx.ai/trace-analysis/grokos).

| | |
|--|--|
| **Meta** | https://nova.teachx.ai/trace-analysis/grokos |
| **Issues** | https://nova.teachx.ai/trace-analysis/grokos/-/issues |
| **Language** | Cap'n SoT: [grokos-schema](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-schema) via Meson subproject/wrap; `schema/SCHEMA_PIN` is publish provenance only |
| **Product API** | Cap'n methods (`policyd_check_shell`, `policyd_status`, …) |
| **C helpers** | `include/grok-policyd/supervisor.h` |

## Cap'n product API (`interface Policyd`)

Schema SoT: `grokos-schema` (`policy.capnp` + `util.capnp`). Cap'n is **always**
linked. One method per domain (no ok|err unions, no CheckBody union). Params
message in / result message out — zero-copy mappable segments across agent,
sessiond, shell.

| Method | Params root | Result root |
|--------|-------------|-------------|
| `status` | — | `PolicydStatus` |
| `checkSeat` | `SeatCheck` | `PolicyDecision` |
| `checkModel` | `ModelCheck` | `PolicyDecision` |
| `checkPath` | `PathCheck` | `PolicyDecision` |
| `checkShell` | `ShellCheck` | `PolicyDecision` |
| `checkRisk` | `RiskCheck` | `PolicyDecision` |
| `checkAudio` | `AudioCheck` | `PolicyDecision` |
| `admitSeat` / `admitModel` | `AdmitSeat` / `AdmitModel` | `PolicyDecision` |
| `agentStatus` | `AgentQuery` | `AgentStatus` |
| `reloadShellPack` | `ReloadShellPack` | `PolicyDecision` |

- **Identity**: `Util.AgentId` bits only.
- **Shell**: `ShellCheck.argv : List(Text)` (spawn argv).
- **Protocol failure**: `PolicyDecision.decision = deny` (fail closed).
- C entry points are `void` and always write a Cap'n result message (or NULL on OOM).

```c
#include <grok-policyd/supervisor.h>

/* in: Cap'n ShellCheck root; out: Cap'n PolicyDecision root */
uint8_t *out = NULL;
size_t out_len = 0;
policyd_check_shell(sup, shell_msg, shell_len, &out, &out_len);
/* decode PolicyDecision from out; free(out) */
```

`policyd_policy_check` is CLI string bridge only.

### Decision table (by method)

| Method | allow when | deny / prompt |
|--------|------------|---------------|
| checkSeat | publishRun / readRun / listRuns / listEvents **and** a running supervisor slot | unknown action, unset AgentId, unknown or non-running id → deny |
| checkModel | running supervisor slot (admit plane) | unset AgentId, unknown or non-running id → deny |
| checkPath | read/write under workspace | outside → deny; delete → prompt |
| checkShell | cwd under workspace; content pack: python via uv+PEP723; deny sudo/curl\|sh/banned PMs/dangerous git | bare python / missing PEP 723 / danger runners → deny |
| checkRisk | — | secretExport → deny; other risk → prompt |
| checkAudio | `GROKOS_POLICYD_AUDIO_ALLOW` fixture (all `AudioAction`; CI/dogfood only — leave unset in production) | product pack (`audio-check`): micOpen/alwaysListen/networkStt/inject **deny**; listenArm **prompt**; unknown **deny**. Host: `DENY_ALL` wins over fixture. No PCM. meta #97 |

Lexical paths: absolute only; reject `//`, `.`, `..`. No `realpath`.

`GROKOS_POLICYD_DENY_ALL` forces deny on every Cap'n method that returns
`PolicyDecision` (checks, admits, `reloadShellPack`) and on the CLI/string
eval path. It wins over `GROKOS_POLICYD_AUDIO_ALLOW`.

## Build / test / coverage (pixi only)

```bash
# once: https://pixi.sh
pixi install --locked
pixi run env-info
just local-ci                 # host/product gates via tools ci-kit (meta #140)
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


## Layout law

Root **README.md only**. No `docs/`, no satellite handbooks, no Makefile. meson→just→pixi.

## License

Apache-2.0. See `LICENSE` and `third_party/NOTICE`.

## Deny-all and audio fixture (tests / lockdown)

Set `GROKOS_POLICYD_DENY_ALL=1` (or `true`/`yes`) to force deny on every Cap'n
`PolicyDecision` entry (and the CLI/string `policy_check` path). Used to prove
agent/sessiond fail closed under a hard seat. Unset for normal allowlists.

Set `GROKOS_POLICYD_AUDIO_ALLOW=1` only in CI/dogfood to allow all `AudioAction` on `checkAudio`. Leave unset in production images. `DENY_ALL` still wins when both are set.

### Schema SoT (Meson)

Policyd consumes Cap'n IDL **only** through a resolved `schemadir`. There is
**no committed** `schema/*.capnp` fork (meta #105). That is the failure #131
prevented: wrap miss → silent compile of a stale local IDL tree while
session/agent spoke a newer tag.

1. **Monorepo dogfood** (preferred when packages sit side-by-side):

   ```bash
   ln -sfn ../../grokos-schema subprojects/grokos-schema
   ```

2. **Wrap** (`subprojects/grokos-schema.wrap`): **schema-v0.3.10** /
   `65d9669` (meta #126 / #131). CI `publish:lib` / `build:pixi` run
   `scripts/ci-seed-schema.sh` then `-Dschema_require_sot=true`. The published
   `.a` is that wrap revision. Wrap clone without credentials is for local
   meson only (`required: false`).

3. **pkg-config** `grokos-schema` (`schemadir=…`) when the schema package is
   installed on the system.

4. **Offline dogfood only:** materialize untracked `schema/*.capnp` next to
   committed `schema/SCHEMA_PIN`:

   ```bash
   grokos-schema/scripts/vendor-into.sh --dest schema --pin
   ```

   Without subproject/pkg-config and without that materialize, meson fails
   closed (no second SoT in git).

`schema/SCHEMA_PIN` is **publish provenance** (uploaded next to
`libgrok_policyd.a`), lockstep with the wrap SHA — not a hand-edited IDL tree.
Consumers compare that SHA to the product pin. A missing stamp after the wrap
moved cannot prove `65d9669` and must not link quietly.

`scripts/gen-capnp-c.sh` always reads `policy.capnp` and `util.capnp` from the
same schemadir (no mixed sources). Staged IDL installs under
`$prefix/share/grok-policyd/` from the codegen custom_target (Meson forbids
`install_data` of nested-subproject files).

## Playground (WASM, multi-pack; not product)

Interactive probe for Cap'n `checkShell` / path / seat / risk with bit-identical
WASM TCB, optional TRACE, and the **same multi-pack load path as product**:

- Default `GROKOS_POLICYD_JANET_PACK=/policy/shell.janet:/policy/packs.d`
- Composition deny > prompt > allow across packs that define the entry
- Author mode reloads a colon list (not a single file only)

```bash
# Prebuilt: playground/dist-wasm/ (parity/site need no emsdk)
pixi run playground-parity
pixi run playground-build
# Full WASM rebuild: emcc on rg.terra only
pixi run playground-wasm
```

Details: [`playground/README.md`](playground/README.md).
