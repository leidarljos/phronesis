# AGENTS.md — grok-policyd

1. Read **meta** rules first: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md
2. **Claim work on meta issues**: https://nova.teachx.ai/trace-analysis/grokos/-/issues
3. This repo implements code for meta issues tagged for **grok-policyd**.
4. Every MR must link `trace-analysis/grokos#N`.
5. No secrets, no proprietary product source paste-ins, no silent stubs.
6. Fail closed on security/policy paths.
7. Prefer host unit/contract tests; use fake model for goal-path tests.

Parent meta: https://nova.teachx.ai/trace-analysis/grokos

## CRITICAL :: Meson is the only build system

- **Canonical build: Meson.** Root file is `meson.build`. Entry point: `pixi run test` / `pixi run build`.
- **Do not create, restore, or maintain a root `Makefile`** (or CMakeLists, autotools, ad-hoc `cc` scripts) for this package. If you are about to write a Makefile, stop.
- **Do not** reintroduce `make test` / `make lib` / `make test-wire` recipes. Those were deleted on purpose.
- Configure + build + test:

  ```bash
  pixi install --locked
  pixi run test          # meson setup/compile + meson test (cmocka + smokes)
  # or:
  meson setup build
  meson compile -C build
  meson test -C build --print-errorlogs
  ```

- Schema codegen is a Meson `custom_target` (`scripts/gen-capnp-c.sh` → `policy.capnp.{c,h}` under the builddir). SoT remains `schema/policy.capnp`. Never commit generated `.c`/`.h`.

## CRITICAL :: cmocka is the only host C unit framework

This rule has been violated repeatedly. Follow it literally.

- **Every host C unit test** lives under `tests/`, is a cmocka group (`cmocka_run_group_tests_name` + `run_*_tests()` in `harness.h`), and is linked into **one** binary: `supervisor_test` (Meson target).
- **`tests/test_main.c`** is the only `main()` among unit tests. It only calls `run_*_tests()`.
- **Forbidden (delete on sight, do not reintroduce):**
  - A second C test binary (`test_wire_frame`, `test_*` with its own `main`)
  - Homegrown `CHECK()` / `ASSERT()` / `exit(1)` harnesses
  - Unity, Criterion, Check, Google Test, TAP mini-frameworks, or "just a small main for wire"
  - `make test-wire` or any target that bypasses cmocka
  - Splitting wire/Cap'n/nng coverage out of the cmocka suite
- **Allowed non-cmocka automation (bash only, not C unit tests):**
  - `scripts/check_shared_libs.sh` — TCB link split
  - `scripts/smoke_serve_signal.sh` — serve SIGTERM
  - `scripts/check-version.sh` — version single-source
  These run via `meson test`, not as a parallel C framework.
- New wire/codec/serve coverage → add a cmocka case in `tests/test_wire_*.c` and register it in `test_main.c` + `harness.h`. Never a standalone program.

## Signed commits

Required. Never set `commit.gpgsign false`. Use `./scripts/setup-commit-signing.sh` if commit fails on signing.

## Build (pixi)

Use **pixi** (see `pixi.toml`):

```bash
pixi install --locked
pixi run test    # or: pixi run ci
```

Meson is the recipe backend. Do not invent ad-hoc host toolchains for dogfood.

## C style (this package)

Follow Robert C. Seacord, *Effective C* (and CERT C where it overlaps): check
library returns, no silent integer wrap on sizes/growth, async-signal-safe
handlers only set flags, free on every error path, bounds on copies into fixed
buffers. Fail closed on policy/ACL paths.

## Link split (do not "simplify" by linking host into TCB)

| Artifact | Links |
|----------|--------|
| `libgrok_policyd` | supervisor ABI + path helpers only. **No** nng / libsystemd / libcap / c-capnproto. |
| `grok-policyd` CLI + wire | nng + c-capnproto + **optional** libsystemd/libcap (Meson `policyd_wire`). |
| `supervisor_test` | TCB + wire + **cmocka** + host deps. |

`scripts/check_shared_libs.sh` enforces the TCB `.so` DT_NEEDED gate.

## Host backend (Unix seat first)

Product target is a **Unix/Linux seat** (Windows/macOS later). Cap'n serve is
**nng-native** (recv timeout + stop flag) on all builds.

- `src/host_posix.c` — signals only (`-Dsystemd=disabled` or auto miss).
- `src/host_linux_systemd.c` — sd_notify READY/STOPPING/WATCHDOG + optional libcap (`-Dsystemd=auto|enabled`).
- **Do not** put serve I/O back on `sd_event` / epoll. Host is notify/watchdog/caps only.
- Meson options: `systemd` and `libcap` features (default `auto`).
- Process engine stays POSIX (`fork`/`setpgid`/process-group); cgroup v2 is Linux best-effort.

See `docs/source/host-backends.rst`.
