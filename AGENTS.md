# AGENTS.md — grok-policyd

1. Meta: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md
2. Tag **grok-policyd**; MR links `trace-analysis/grokos#N`.
3. **Cap'n is the language.** Product API is `grok_policyd_handle_capnp` (FFI).
4. **No policyd.sock / serve.** Seat nng stays on sessiond.
5. Schema SoT: `schema/policy.capnp`. Do not invent parallel wire DTOs.
6. cmocka only: one `supervisor_test`; only `tests/test_main.c` has `main`.
7. Signed commits; fail closed on policy paths.

```bash
pixi install --locked && pixi run test
```
