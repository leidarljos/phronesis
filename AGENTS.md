# AGENTS.md — grok-policyd

1. Read **meta** rules first: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md  
2. **Claim work on meta issues**: https://nova.teachx.ai/trace-analysis/grokos/-/issues  
3. This repo implements code for meta issues tagged for **grok-policyd**.  
4. Every MR must link `trace-analysis/grokos#N`.  
5. No secrets, no proprietary product source paste-ins, no silent stubs.  
6. Fail closed on security/policy paths.  
7. Prefer host unit/contract tests; use fake model for goal-path tests.

Parent meta: https://nova.teachx.ai/trace-analysis/grokos  

## Signed commits

Required. Never set `commit.gpgsign false`. Use `./scripts/setup-commit-signing.sh` if commit fails on signing.

## Build

Use **pixi** (see `pixi.toml`):

```bash
pixi install --locked
pixi run test    # or: pixi run ci
```

`Makefile` is the recipe backend; do not invent ad-hoc host toolchains for dogfood.

