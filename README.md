# phronesis

In-process policy library for a multi-agent seat. Callers link `libphronesis`
and send Cap'n messages into the typed C entry points (`phronesis_check_shell`,
`phronesis_check_path`, `phronesis_check_seat`, …). Every check returns
a Cap'n `PolicyDecision` (`deny` / `allow` / `prompt`) plus a `PolicyReason`
code. Protocol failures deny.

| | |
|--|--|
| **Language** | Cap'n interface: `schema/policy.capnp` + `schema/util.capnp` in this tree |
| **Product API** | typed C entry points in `include/phronesis/supervisor.h` |
| **C helpers** | `include/phronesis/supervisor.h` |
| **License** | MIT (`LICENSE`); third-party notes in `NOTICE` |

## Build (no private remotes)

Needs: `meson` ≥ 1.3, `ninja`, `pkg-config`, `capnp` (compiler), `cmocka`.
If `c-capnproto` / `capnpc-c` are not installed, Meson fetches
https://github.com/HaoZeke/c-capnproto. `capnp-janet` is the same via wrap.

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Or `just meson-test`. Embedders find the interface at
`pkg-config --variable=schemadir phronesis` after install.

`pixi install --locked` still uses a private conda channel for
`c-capnproto`. The Meson path above is the standalone door.

## Cap'n product API (`interface Policyd`)

Public interface: `schema/policy.capnp` + `schema/util.capnp`. Cap'n is **always**
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
#include <phronesis/supervisor.h>

/* in: Cap'n ShellCheck root; out: Cap'n PolicyDecision root */
uint8_t *out = NULL;
size_t out_len = 0;
phronesis_check_shell(sup, shell_msg, shell_len, &out, &out_len);
/* decode PolicyDecision from out; free(out) */
```

`phronesis_policy_check` is CLI string bridge only.

### Decision table (by method)

| Method | allow when | deny / prompt |
|--------|------------|---------------|
| checkSeat | publishRun / readRun / listRuns / listEvents | unknown action → deny |
| checkModel | always (admit plane) | — |
| checkPath | read/write under workspace | outside → deny; delete → prompt |
| checkShell | cwd under workspace; content pack: python via uv+PEP723; deny sudo/curl\|sh/banned PMs/dangerous git | bare python / missing PEP 723 / danger runners → deny |
| checkRisk | — | secretExport → deny; other risk → prompt |
| checkAudio | `GROKOS_POLICYD_AUDIO_ALLOW` fixture (all `AudioAction`; CI/dogfood only — leave unset in production) | product pack (`audio-check`): micOpen/alwaysListen/networkStt/inject **deny**; listenArm **prompt**; unknown **deny**. Host: `DENY_ALL` wins over fixture. No PCM. meta #97 |

Lexical paths: absolute only; reject `//`, `.`, `..`. No `realpath`.

`GROKOS_POLICYD_DENY_ALL` forces deny on the CLI/string eval path and on `checkAudio` (it wins over `GROKOS_POLICYD_AUDIO_ALLOW`). Not all Cap'n methods consult it yet.

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

GitHub job **`meson-test`** builds, runs `meson test`, and uploads `libphronesis.a`
plus `schema/SCHEMA_PIN`, `policy.capnp`, and `util.capnp`. A tag `vX.Y.Z` that
matches `VERSION` publishes those files as a GitHub release.

Job **`mutation:mull`**: Ubuntu 24.04, system Clang + Mull, pixi for deps only
(`CC=/usr/bin/clang-19`). Path-triggered on policy/Cap'n changes and on
schedules. Prints the IDE survivor list at the end of the job log; full report
under artifact `build-mull/mull-report/`.


## Layout law

Root **README.md only**. No `docs/`, no satellite handbooks, no Makefile. meson→just→pixi.

## License

MIT for first-party code. See `LICENSE` and `NOTICE`.

## Deny-all and audio fixture (tests / lockdown)

Set `GROKOS_POLICYD_DENY_ALL=1` (or `true`/`yes`) to force deny on the CLI/string `policy_check` path and on Cap'n `checkAudio`. Used to prove agent/sessiond fail closed under a hard seat. Unset for normal allowlists.

Set `GROKOS_POLICYD_AUDIO_ALLOW=1` only in CI/dogfood to allow all `AudioAction` on `checkAudio`. Leave unset in production images. `DENY_ALL` still wins when both are set.

### Cap'n interface (Meson)

This library compiles `schema/policy.capnp` and `schema/util.capnp`. Those
files are the public interface. Seat `session.capnp` is not part of this
package.

```bash
meson setup build
meson compile -C build
meson test -C build
```

Optional: copy newer files from a sibling schema tree with `just sync-schema`.
Optional: `-Dschema_dir=/path` to point codegen at another directory that
contains both files.

`scripts/gen-capnp-c.sh` reads both files from one directory (no mixed
sources). Staged IDL installs under `$prefix/share/phronesis/`.

A stranger with `meson`, `ninja`, `capnp`, `capnpc-c`, `c-capnproto`, and
`cmocka` can build from this tree without private remotes. `pixi install
--locked` still needs the current channel list (private `c-capnproto`
package) until that lock is rebuilt on conda-forge.

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
