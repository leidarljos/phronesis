#!/usr/bin/env bash
# TCB .so must not pull a socket/host stack (nng/systemd/libcap).
# Cap'n pure-C runtime is libcapnp_c from the c-capnproto package (not vendored).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${POLICYD_BUILD_DIR:-$ROOT/build}"
SO=
if [[ -e "$BUILD/libgrok_policyd.so" ]]; then
  SO="$BUILD/libgrok_policyd.so"
elif compgen -G "$BUILD/libgrok_policyd.so.*" >/dev/null; then
  SO=$(ls -1 "$BUILD"/libgrok_policyd.so.* | head -1)
fi
if [[ -L "${SO:-}" ]]; then
  target=$(readlink "$SO")
  if [[ "$target" != /* ]]; then SO="$(dirname "$SO")/$target"; else SO="$target"; fi
fi
test -n "${SO:-}" && test -f "$SO" || { echo "missing shared lib under $BUILD" >&2; exit 1; }
echo "checking $SO"
need=$(readelf -d "$SO" 2>/dev/null | awk '/NEEDED/ {print $5}' | tr -d '[]' || true)
echo "DT_NEEDED: $need"
# Forbidden host/socket stacks. Match full soname tokens (libcap != libcapnp_c).
while read -r soname; do
  [[ -z "$soname" ]] && continue
  case "$soname" in
    libnng.so*|libuv.so*|libsystemd.so*|libcap.so*)
      echo "error: TCB .so must not NEEDED $soname (Cap'n is in-process FFI, not a peer)" >&2
      exit 1
      ;;
  esac
done <<<"$need"
# Must link the real Cap'n C runtime (package), not a private static copy.
if ! echo "$need" | grep -qE 'libcapnp_c\.so'; then
  echo "error: expected DT_NEEDED libcapnp_c.so (c-capnproto package)" >&2
  exit 1
fi
# Public Cap'n entry must exist
if ! nm -D "$SO" 2>/dev/null | grep -q 'grok_policyd_handle_capnp'; then
  if ! nm "$SO" 2>/dev/null | grep -q 'grok_policyd_handle_capnp'; then
    echo "error: missing exported grok_policyd_handle_capnp" >&2
    exit 1
  fi
fi
echo "ok: Cap'n FFI links libcapnp_c; no socket host stack"
