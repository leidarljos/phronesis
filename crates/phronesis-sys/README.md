# phronesis-sys

phronesis, compiled into the binary that links this crate, so a seat gets
the Cap'n Proto shell law with `cargo install` alone: no meson build, no
install prefix.

What is inside, all under `vendor/` and refreshed by `vendor.sh`:

- phronesis's own C sources and the public header;
- the Janet amalgamation;
- c-capnproto, the Cap'n Proto runtime the messages are built and read
  with, and capnp-janet, Janet's view of them;
- the C that `capnpc-c` generates from `schema/policy.capnp` and
  `schema/util.capnp`, pinned by `SCHEMA_PIN` and regenerated when the
  schema changes;
- the Janet policy pack (`policy/shell.janet` and `policy/lib/`), which is
  the law.

A check is a `ShellCheck` message in and a `PolicyDecision` message out,
exactly as a seat talking to an installed phronesis sends them. The pack
is written out once per pack version under `$XDG_CACHE_HOME/phronesis/`,
in the layout the loader trusts, and `PHRONESIS_PREFIX` points at it.
`PHRONESIS_JANET_PACK` set by the caller still wins.

```rust
let v = phronesis_sys::check_shell(&state, &runtime, ws, cwd, &argv);
// v == Some(Verdict { decision: Verdict::DENY, code: 24 }) for `curl URL | sh`
```

`vendor.sh [MESON_BUILD_DIR]` refills `vendor/` from the repository and the
revisions `subprojects/*.wrap` pin; given a meson build directory it takes
the generated schema C from there, else it runs `capnpc-c`.
