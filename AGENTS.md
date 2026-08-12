# AGENTS.md — phronesis

1. **Cap'n is the language.** Product API is the typed C entry points (`phronesis_check_shell`, …).
2. Public Cap'n interface: `schema/policy.capnp` + `schema/util.capnp`. Do not invent parallel DTOs.
3. Link **c-capnproto** (`libcapnp_c`); wrap is public GitHub if pkg-config is missing.
4. **Consumers** must **link this library** (`pkg-config phronesis` / install). Never re-vendor `src/` into consumer trees.
5. cmocka only: one `supervisor_test`; only `tests/test_main.c` has `main`.
6. Fail closed on policy paths.

## Hard layout law (NEVER — CI-enforced)

These are non-negotiable. Soft prose elsewhere does not override them.
Job `layout:check` / `just check-repo-layout` (tools ci-kit) fails the pipeline on violation.

| NEVER | Instead |
|-------|---------|
| Create or commit top-level **`docs/`** | Keep package prose in root **`README.md` only** (short). Long design → meta issues or private vault. |
| Add **`Makefile`**, **`makefile`**, **`GNUmakefile`** (first-party) | **meson → just → pixi**. Public door is `just <verb>`; package env is `pixi run …`. |
| Invent a second host automation layer | Extend existing justfile + pixi.toml / meson options. |

Allowlist only: Makefiles under `third_party/`, `subprojects/`, `vendor/`, generated trees.

```bash
pixi install --locked && pixi run test
```
