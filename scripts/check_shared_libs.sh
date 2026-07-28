#!/usr/bin/env bash
# TCB .so must not pull a socket/host stack (nng/systemd/libcap).
# Cap'n runtime is static-linked from third_party/c-capnproto.
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
for lib in libnng libuv libsystemd libcap; do
  if echo "$need" | grep -q "$lib"; then
    echo "error: TCB .so must not NEEDED $lib (Cap'n is in-process FFI, not a peer)" >&2
    exit 1
  fi
done
# Public Cap'n entry must exist
if ! nm -D "$SO" 2>/dev/null | grep -q 'grok_policyd_handle_capnp'; then
  if ! nm "$SO" 2>/dev/null | grep -q 'grok_policyd_handle_capnp'; then
    echo "error: missing exported grok_policyd_handle_capnp" >&2
    exit 1
  fi
fi
echo "ok: Cap'n FFI lib deps minimal"
