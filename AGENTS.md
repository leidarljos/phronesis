# AGENTS.md — grok-policyd

1. Read **meta** rules first: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md
2. Claim work on **meta issues**: https://nova.teachx.ai/trace-analysis/grokos/-/issues
3. This repo implements code for meta issues tagged for **grok-policyd**.
4. Every merge request must link `trace-analysis/grokos#N`.
5. No secrets, no proprietary product source paste-ins, no silent stubs.
6. Prefer host unit/contract tests; goal-path tests use a **fake model**.
7. Root [`README.md`](./README.md) is the package map. Keep docs here simple — do not reintroduce a multi-file docs tree.

Parent meta: https://nova.teachx.ai/trace-analysis/grokos

## Documentation

- Write what the package **is** and **does**; avoid long “what this is not” catalogs.
- No debate scaffolding or workshop decision IDs.
- No internal thought trail.
- Document **our** glue only; link or name upstream tools instead of rewriting their manuals.
- Prefer plain nouns; keep the single root `README.md` as the package doc map.

## Signed commits

Required. Never set `commit.gpgsign false`. Use `./scripts/setup-commit-signing.sh` if commit fails on signing.

## Build

```bash
pixi install --locked
pixi run test    # or: pixi run ci
```

## CRITICAL :: cmocka is the only host C unit framework

- Every host C unit test lives under `tests/`, is a cmocka group, linked into **one** binary: `supervisor_test`.
- **`tests/test_main.c`** is the only `main()` among unit tests.
- **Forbidden:** second C test binary, homegrown CHECK harnesses, bypassing cmocka.
- New Cap'n/serve coverage → cmocka case in `tests/test_wire_*.c` registered in `test_main.c`.

## Link split

| Artifact | Links |
|----------|--------|
| `libgrok_policyd` | supervisor ABI + path helpers. **No** nng / libsystemd / libcap. |
| `grok-policyd` CLI + serve | nng + c-capnproto + optional systemd/libcap |

`scripts/check_shared_libs.sh` enforces the TCB `.so` DT_NEEDED gate.

## Host

`src/host.c` — SIGTERM/SIGINT stop flag; optional `sd_notify` / libcap via Meson features. Serve I/O is nng-native only.

Fail closed on security and policy paths.
