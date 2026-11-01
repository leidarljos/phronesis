# Retarget a host seat from ljos-policyd to phronesis

phronesis is the in-process policy library. After the `ljos-policyd`
package is dropped, host consumers link this library, call its C
symbols, and take `policy.capnp` + `util.capnp` from the phronesis
release they link.

Cap'n file names and types stay `policy.capnp`, `util.capnp`,
`interface Policyd`, `PolicyDecision`.

## Link

| Was | Now |
|-----|-----|
| `pkg-config ljos-policyd` | `pkg-config phronesis` |
| `libljos_policyd.a` / `libljos_policyd.so` | `libphronesis.a` / `libphronesis.so` |
| `#[link(name = "ljos_policyd")]` | `#[link(name = "phronesis")]` |
| `-lljos_policyd` | `-lphronesis` |
| `include/ljos-policyd/supervisor.h` | `include/phronesis/supervisor.h` |
| `$prefix/share/ljos-policyd/` | `$prefix/share/phronesis/` |
| `LJOS_POLICYD_DIR` pointing at a prefix with `libljos_policyd.a` | directory that contains `libphronesis.a` (session `link_static_c.rs` / `build.rs`) |

GitHub snapshot (no version tag):
https://github.com/leidarljos/phronesis/releases/tag/snapshot

Version tags `vX.Y.Z` publish the same four files: `libphronesis.a`,
`SCHEMA_PIN`, `policy.capnp`, `util.capnp`.

## C symbols

`API_VERSION` is 3. Every former `ljos_policyd_*` / `ljos_supervisor_*`
entry is `phronesis_*`. Types and status macros follow.

| Was | Now |
|-----|-----|
| `ljos_supervisor_t` | `phronesis_supervisor_t` |
| `ljos_supervisor_open` / `_close` / `_start` / `_stop` / `_status` / `_log` | `phronesis_supervisor_open` and the same suffixes |
| `ljos_policyd_status` | `phronesis_status` |
| `ljos_policyd_check_seat` / `_check_model` / `_check_path` / `_check_shell` / `_check_risk` / `_check_audio` | `phronesis_check_seat` and the same suffixes |
| `ljos_policyd_admit_seat` / `_admit_model` | `phronesis_admit_seat` / `phronesis_admit_model` |
| `ljos_policyd_agent_status` | `phronesis_agent_status` |
| `ljos_policyd_reload_shell_pack` | `phronesis_reload_shell_pack` |
| `ljos_policy_check` / `ljos_policy_shell_pack_reload` | `phronesis_policy_check` / `phronesis_shell_pack_reload` |
| `LJOS_OK`, `LJOS_ERR_*`, `LJOS_DECISION_*`, `LJOS_REASON_*` | `PHRONESIS_OK`, `PHRONESIS_ERR_*`, `PHRONESIS_DECISION_*`, `PHRONESIS_REASON_*` |
| `LJOS_ID_MAX` / `LJOS_PATH_MAX` / … | `PHRONESIS_ID_MAX` / `PHRONESIS_PATH_MAX` / … |

Rust FFI structs that mirrored `LjosSupervisor` / `LJOS_OK` must use the
new names and sizes from `include/phronesis/supervisor.h`.

## Environment

The library reads these keys. Dual names are not kept in product code.

| Was | Now |
|-----|-----|
| `SEATENV_STATE_DIR` | `PHRONESIS_STATE_DIR` |
| `SEATENV_RUNTIME_DIR` | `PHRONESIS_RUNTIME_DIR` |
| `SEATENV_ACTION_LOG` | `PHRONESIS_ACTION_LOG` |
| `SEATENV_POLICYD_JANET_PACK` | `PHRONESIS_JANET_PACK` |
| `SEATENV_POLICYD_DENY_ALL` | `PHRONESIS_DENY_ALL` |
| `SEATENV_POLICYD_AUDIO_ALLOW` | `PHRONESIS_AUDIO_ALLOW` |
| `SEATENV_POLICYD_PACK_ROOT` | `PHRONESIS_PACK_ROOT` |
| `SEATENV_POLICYD_DEV_PACK` | `PHRONESIS_DEV_PACK` |
| `SEATENV_PREFIX` | `PHRONESIS_PREFIX` |

Default XDG leaf is `phronesis` (was `host`). Default cgroup child
is `phronesis-<agent-id>` (was `host-<agent-id>`).

Seat-side names the library does **not** read (callers pass dirs into
`phronesis_supervisor_open`):

| Seat / session env (still host-owned until those trees retarget) | Typical use |
|-----|-----|
| `SEATENV_POLICYD_STATE_DIR` | Passed as `state_dir` to `open` |
| `SEATENV_POLICYD_RUNTIME_DIR` | Passed as `runtime_dir` to `open` |
| `SEATENV_POLICYD_AGENT` | Agent id hex for Cap'n `AgentId` |
| `SEATENV_RUN_ID` | Seat run id; library does not authenticate it |

When retargeting, either keep passing those paths into `open`, or set
`PHRONESIS_STATE_DIR` / `PHRONESIS_RUNTIME_DIR` and pass NULL.

## Schema

Compile and ship `schema/policy.capnp` + `schema/util.capnp` from the
phronesis tree (or release asset). `SCHEMA_PIN` rides next to
`libphronesis.a`. Stop taking policy/util from `host-schema` for
this library; seat `session.capnp` stays in the session package.

`pkg-config --variable=schemadir phronesis` is `$prefix/share/phronesis`.

## Consumers to retarget (follow-up, not this tree)

1. **Agent FFI** — `host-agent/src/policyd.rs`: `#[link(name = "ljos_policyd")]`,
   `ljos_supervisor_*` / `ljos_policyd_*`, `SEATENV_POLICYD_STATE_DIR` /
   `SEATENV_POLICYD_RUNTIME_DIR` / `SEATENV_POLICYD_DENY_ALL` /
   `SEATENV_POLICYD_JANET_PACK`. Also `tool_policy.rs`, `pack_mailbox.rs`.
2. **Session launch** — `host-session/crates/host-sessiond/src/policyd_ffi.rs`
   and `link_static_c.rs` / `build.rs`: static `libljos_policyd.a`,
   `LJOS_POLICYD_DIR`. `jail.rs` and `launch.rs` export
   `SEATENV_POLICYD_*` / `SEATENV_STATE_DIR` into the seat.
3. **Shell link** — `host-shell/product/src/policyd.rs` same FFI as agent;
   `LJOS_POLICYD_DIR` / `pkg-config` in `build.rs`.
4. **Schema pin** — `host-schema` currently claims policy/util for the
   seat stack. After drop, host-schema keeps `session.capnp`; policy/util
   come from the phronesis release the binaries link.
5. **Meta package list** — `host/packages/MANIFEST.yml` entry `ljos-policyd`
   (`example-group/host-packages/ljos-policyd`). Replace with a phronesis
   checkout or drop the package and consume the GitHub artifact.

## Suggested order

1. Point consumer `build.rs` at `libphronesis.a` + `libcapnp_c` +
   `libcapnp_janet` from a phronesis install or snapshot.
2. Rename FFI symbols and `LJOS_*` constants in agent, session, shell.
3. Set `PHRONESIS_DENY_ALL` / `PHRONESIS_AUDIO_ALLOW` /
   `PHRONESIS_JANET_PACK` in dogfood scripts (session skills today set
   `SEATENV_POLICYD_*`).
4. Remove `ljos-policyd` from MANIFEST once those three packages build
   and their policy tests pass against phronesis.

## Non-goals

Dual env aliases in this library. OpenMandriva
`host-packaging/ljos-policyd`. Playground WASM rebuild.
