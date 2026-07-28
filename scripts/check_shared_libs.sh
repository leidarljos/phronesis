#!/usr/bin/env bash
# Fail if libgrok_policyd.so grows unexpected NEEDED deps.
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
# Must stay free of network/IPC stacks (policy is in-process).
for lib in libnng libuv libsystemd libcap libcapnp_c libcapnp; do
  if echo "$need" | grep -q "$lib"; then
    echo "error: TCB .so must not NEEDED $lib" >&2
    exit 1
  fi
done
echo "ok: TCB shared lib deps minimal"
