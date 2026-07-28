# AGENTS.md — grok-policyd

1. Read **meta** rules first: https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md (or sibling meta checkout `AGENTS.md` on `distilled-docs`).  
2. Claim work on **meta issues**: https://nova.teachx.ai/trace-analysis/grokos/-/issues  
3. This repo implements code for meta issues tagged for **grok-policyd**.  
4. Every merge request must link `trace-analysis/grokos#N`.  
5. No secrets, no proprietary product source paste-ins, no silent stubs.  
6. Prefer host unit/contract tests; goal-path tests use a **fake model** (no live keys).  
7. Root [`README.md`](./README.md) is the package map. Keep docs here simple — do not reintroduce a multi-file docs tree.

Parent meta: https://nova.teachx.ai/trace-analysis/grokos

## Documentation

When editing docs (including this README):

- Write what the package **is** and **does**; avoid long “what this is not” catalogs.  
- No debate scaffolding or workshop decision IDs.  
- No internal thought trail.  
- Document **our** glue only; link or name upstream tools instead of rewriting their manuals.  
- Prefer plain nouns; keep the single root `README.md` as the package doc map.

## Signed commits

Required. Never set `commit.gpgsign false`. Use `./scripts/setup-commit-signing.sh` if commit fails on signing.

## Build

Use **pixi** (`pixi.toml`):

```bash
pixi install --locked
pixi run test    # or: pixi run ci
```

`Makefile` is the recipe backend; do not invent ad-hoc host toolchains for dogfood.

Fail closed on security and policy paths.
