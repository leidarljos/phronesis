# Cap'n policy peer fixtures (#25 / Slice C)

Binary Cap'n **message bodies** (no GKPP header) produced by `make test-wire`
via `src/wire/capnp_min.c` (hand codec — **not** full capnp-c).

| File | Content |
|------|---------|
| `status_request.bin` | PolicyEnvelope request op=status |
| `status_response.bin` | PolicyEnvelope response ok=status |
| `status_response.hex` | hex dump of status_response |
| `check_allow_response.bin` | PolicyEnvelope response ok=check allow |

Regenerate:

```bash
make test-wire
# or: POLICYD_FIXTURE_DIR=/path make test-wire
```

Session crate may copy or path-search these for optional decode tests.
Live interop gate is `scripts/smoke_policyd_wire.sh` (Rust client ↔ `serve`).
