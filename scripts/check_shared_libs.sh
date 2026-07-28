#!/usr/bin/env bash
# Fail if libgrok_policyd.so DT_NEEDED lists CLI host stack.
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
  if [[ "$target" != /* ]]; then
    SO="$(dirname "$SO")/$target"
  else
    SO="$target"
  fi
fi
test -n "${SO:-}" && test -f "$SO" || {
  echo "missing shared lib under $BUILD (meson compile first)" >&2
  exit 1
}
echo "checking $SO"
need=$(readelf -d "$SO" 2>/dev/null | awk '/NEEDED/ {print $5}' | tr -d '[]' || true)
echo "DT_NEEDED: $need"
bad=
# CLI/host wire stack only — never DT_NEEDED of the TCB .so.
for lib in libnng libuv libsystemd libcap libcapnp_c libcapnp; do
  if echo "$need" | grep -q "$lib"; then
    echo "error: shared TCB lib must not NEEDED $lib (CLI/wire only)" >&2
    bad=1
  fi
done
# pkg-config must not list them either
pc="$BUILD/meson-private/grok-policyd.pc"
if [[ ! -f "$pc" ]]; then
  pc="$BUILD/grok-policyd.pc"
fi
# meson may put .pc in meson-uninstalled or root
if [[ ! -f "$pc" ]]; then
  pc=$(find "$BUILD" -name 'grok-policyd.pc' 2>/dev/null | head -1 || true)
fi
if [[ -n "${pc:-}" && -f "$pc" ]]; then
  libs=$(grep '^Libs:' "$pc" || true)
  echo "Libs: $libs ($pc)"
  for lib in nng uv systemd cap capnp_c capnp; do
    if echo "$libs" | grep -q -- "-l$lib"; then
      echo "error: .pc Libs pulls -l$lib (CLI/wire only)" >&2
      bad=1
    fi
  done
else
  echo "warn: grok-policyd.pc not found under $BUILD (skipping .pc Libs check)" >&2
fi
[[ -z "${bad:-}" ]] || exit 1
echo "ok: shared lib and .pc free of CLI/wire host stack"
