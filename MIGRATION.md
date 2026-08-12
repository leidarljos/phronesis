# Retarget a GrokOS seat from grok-policyd to phronesis

phronesis is the in-process policy library. After the `grok-policyd`
package is dropped, GrokOS consumers link this library, call its C
symbols, and take `policy.capnp` + `util.capnp` from the phronesis
release they link.

Cap'n file names and types stay `policy.capnp`, `util.capnp`,
`interface Policyd`, `PolicyDecision`.

## Link

| Was | Now |
|-----|-----|
| `pkg-config grok-policyd` | `pkg-config phronesis` |
| `libgrok_policyd.a` / `libgrok_policyd.so` | `libphronesis.a` / `libphronesis.so` |
| `#[link(name = "grok_policyd")]` | `#[link(name = "phronesis")]` |
| `-lgrok_policyd` | `-lphronesis` |
| `include/grok-policyd/supervisor.h` | `include/phronesis/supervisor.h` |
| `$prefix/share/grok-policyd/` | `$prefix/share/phronesis/` |
| `GROK_POLICYD_DIR` pointing at a prefix with `libgrok_policyd.a` | directory that contains `libphronesis.a` (session `link_static_c.rs` / `build.rs`) |

GitHub snapshot (no version tag):
https://github.com/indynull/phronesis/releases/tag/snapshot

Version tags `vX.Y.Z` publish the same four files: `libphronesis.a`,
`SCHEMA_PIN`, `policy.capnp`, `util.capnp`.

## C symbols

`API_VERSION` is 3. Every former `grok_policyd_*` / `grok_supervisor_*`
entry is `phronesis_*`. Types and status macros follow.

| Was | Now |
|-----|-----|
| `grok_supervisor_t` | `phronesis_supervisor_t` |
| `grok_supervisor_open` / `_close` / `_start` / `_stop` / `_status` / `_log` | `phronesis_supervisor_open` and the same suffixes |
| `grok_policyd_status` | `phronesis_status` |
| `grok_policyd_check_seat` / `_check_model` / `_check_path` / `_check_shell` / `_check_risk` / `_check_audio` | `phronesis_check_seat` and the same suffixes |
| `grok_policyd_admit_seat` / `_admit_model` | `phronesis_admit_seat` / `phronesis_admit_model` |
| `grok_policyd_agent_status` | `phronesis_agent_status` |
| `grok_policyd_reload_shell_pack` | `phronesis_reload_shell_pack` |
| `grok_policy_check` / `grok_policy_shell_pack_reload` | `phronesis_policy_check` / `phronesis_shell_pack_reload` |
| `GROK_OK`, `GROK_ERR_*`, `GROK_DECISION_*`, `GROK_REASON_*` | `PHRONESIS_OK`, `PHRONESIS_ERR_*`, `PHRONESIS_DECISION_*`, `PHRONESIS_REASON_*` |
| `GROK_ID_MAX` / `GROK_PATH_MAX` / … | `PHRONESIS_ID_MAX` / `PHRONESIS_PATH_MAX` / … |

Rust FFI structs that mirrored `GrokSupervisor` / `GROK_OK` must use the
new names and sizes from `include/phronesis/supervisor.h`.

## Environment

The library reads these keys. Dual names are not kept in product code.

| Was | Now |
|-----|-----|
| `GROKOS_STATE_DIR` | `PHRONESIS_STATE_DIR` |
| `GROKOS_RUNTIME_DIR` | `PHRONESIS_RUNTIME_DIR` |
| `GROKOS_ACTION_LOG` | `PHRONESIS_ACTION_LOG` |
| `PHRONESIS_JANET_PACK` | `PHRONESIS_JANET_PACK` |
| `PHRONESIS_DENY_ALL` | `PHRONESIS_DENY_ALL` |
| `PHRONESIS_AUDIO_ALLOW` | `PHRONESIS_AUDIO_ALLOW` |
| `PHRONESIS_PREFIX` | `PHRONESIS_PREFIX` |

Default XDG leaf is `phronesis` (was `grokos`). Default cgroup child
is `phronesis-<agent-id>` (was `grok-<agent-id>`).

Seat-side names the library does **not** read (callers pass dirs into
`phronesis_supervisor_open`):

| Seat / session env (still GrokOS-owned until those trees retarget) | Typical use |
|-----|-----|
| `PHRONESIS_STATE_DIR` | Passed as `state_dir` to `open` |
| `PHRONESIS_RUNTIME_DIR` | Passed as `runtime_dir` to `open` |
| `PHRONESIS_AGENT` | Agent id hex for Cap'n `AgentId` |
| `GROKOS_RUN_ID` | Seat run id; library does not authenticate it |

When retargeting, either keep passing those paths into `open`, or set
`PHRONESIS_STATE_DIR` / `PHRONESIS_RUNTIME_DIR` and pass NULL.

## Schema

Compile and ship `schema/policy.capnp` + `schema/util.capnp` from the
phronesis tree (or release asset). `SCHEMA_PIN` rides next to
`libphronesis.a`. Stop taking policy/util from `grokos-schema` for
this library; seat `session.capnp` stays in the session package.

`pkg-config --variable=schemadir phronesis` is `$prefix/share/phronesis`.

## Consumers to retarget (follow-up, not this tree)

1. **Agent FFI** — `grokos-agent/src/policyd.rs`: `#[link(name = "grok_policyd")]`,
   `grok_supervisor_*` / `grok_policyd_*`, `PHRONESIS_STATE_DIR` /
   `PHRONESIS_RUNTIME_DIR` / `PHRONESIS_DENY_ALL` /
   `PHRONESIS_JANET_PACK`. Also `tool_policy.rs`, `pack_mailbox.rs`.
2. **Session launch** — `grokos-session/crates/grokos-sessiond/src/policyd_ffi.rs`
   and `link_static_c.rs` / `build.rs`: static `libgrok_policyd.a`,
   `GROK_POLICYD_DIR`. `jail.rs` and `launch.rs` export
   `PHRONESIS_*` / `GROKOS_STATE_DIR` into the seat.
3. **Shell link** — `grokos-shell/product/src/policyd.rs` same FFI as agent;
   `GROK_POLICYD_DIR` / `pkg-config` in `build.rs`.
4. **Schema pin** — `grokos-schema` currently claims policy/util for the
   seat stack. After drop, grokos-schema keeps `session.capnp`; policy/util
   come from the phronesis release the binaries link.
5. **Meta package list** — `grokos/packages/MANIFEST.yml` entry `grok-policyd`
   (`trace-analysis/grokos-packages/grok-policyd`). Replace with a phronesis
   checkout or drop the package and consume the GitHub artifact.

## Suggested order

1. Point consumer `build.rs` at `libphronesis.a` + `libcapnp_c` +
   `libcapnp_janet` from a phronesis install or snapshot.
2. Rename FFI symbols and `GROK_*` constants in agent, session, shell.
3. Set `PHRONESIS_DENY_ALL` / `PHRONESIS_AUDIO_ALLOW` /
   `PHRONESIS_JANET_PACK` in dogfood scripts (session skills today set
   `PHRONESIS_*`).
4. Remove `grok-policyd` from MANIFEST once those three packages build
   and their policy tests pass against phronesis.

## Non-goals

Dual env aliases in this library. OpenMandriva
`grokos-packaging/grok-policyd`. Playground WASM rebuild.
