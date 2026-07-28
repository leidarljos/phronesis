# AGENTS.md — grok-policyd

1. Meta rules: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md
2. Meta issues: https://nova.teachx.ai/trace-analysis/grokos/-/issues
3. Tag work **grok-policyd**; MR links `trace-analysis/grokos#N`.
4. Root [`README.md`](./README.md) is the only package map — no multi-file docs tree.
5. **Library only:** no nng Cap'n `serve`, no `policyd.sock`. Seat Cap'n stays on sessiond; policy is in-process FFI/link.
6. cmocka only: one `supervisor_test` binary; only `tests/test_main.c` has `main`.
7. Signed commits; fail closed on policy paths.

```bash
pixi install --locked && pixi run test
```
