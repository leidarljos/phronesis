# phronesis-sys

phronesis, the in-process shell policy library, built from vendored C by
`cargo` alone: the library sources, the Janet amalgamation, the Cap'n
Proto C runtime, the capnp-janet runtime and the C generated from the
schema are under `vendor/`, the pack under `policy/`. No meson, no capnp
compiler and no install prefix at build time; a C compiler is enough.

The crate exposes the C API (`ffi`), a `Supervisor` that opens a seat,
binds it to a workspace and judges one shell line, and the embedded pack,
which `install_pack` writes once to `$XDG_CACHE_HOME/phronesis-sys/<hash>/`
in the layout phronesis trusts and names through `PHRONESIS_JANET_PACK`.

`scripts/vendor-refresh.sh` refreshes `vendor/` from the repository and
the revisions `subprojects/*.wrap` pin; `vendor/PINS` records them. The
pack is shipped as it is in the repository at that revision.
