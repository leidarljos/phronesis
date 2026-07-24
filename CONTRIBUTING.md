# Contributing to grok-policyd

See meta [CONTRIBUTING.md](https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/CONTRIBUTING.md) and [AGENTS.md](https://nova.teachx.ai/trace-analysis/grokos/-/blob/main/AGENTS.md).

**Workflow:** claim meta issue → branch here → MR here with link to meta issue → close meta issue when accepted.

## Signed commits (required)

GrokOS requires verified GPG/SSH commit signatures. Run:

```bash
./scripts/setup-commit-signing.sh
```

Upload the same public key on GitLab (SSH Keys → Authentication & Signing).
Agents must not disable `commit.gpgsign`. See meta `trace-analysis/grokos` CONTRIBUTING for full policy.
