# AGENTS.md — grok-policyd

1. Meta: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md
2. Tag **grok-policyd**; MR links `trace-analysis/grokos#N`.
3. **Cap'n is the language.** Product API is `grok_policyd_handle_capnp` (FFI).
4. Schema SoT: [grokos-schema](https://nova.teachx.ai/trace-analysis/grokos-packages/grokos-schema) via meson subproject/wrap (`schema/SCHEMA_PIN` is pin only; no committed `schema/*.capnp`). Do not invent parallel wire DTOs.
5. Link **c-capnproto** (`libcapnp_c`); do not vendor capn runtime sources.
6. **Consumers** (sessiond/agent/shell) must **link this library** (`pkg-config grok-policyd` / install). Never re-vendor `src/` into consumer trees.
7. cmocka only: one `supervisor_test`; only `tests/test_main.c` has `main`.
8. Signed commits; fail closed on policy paths.

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
