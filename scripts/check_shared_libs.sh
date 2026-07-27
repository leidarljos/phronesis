#!/usr/bin/env bash
# Fail if libgrok_policyd.so DT_NEEDED lists CLI host stack.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SO=
if [[ -e "$ROOT/build/libgrok_policyd.so" ]]; then
  SO="$ROOT/build/libgrok_policyd.so"
elif compgen -G "$ROOT/build/libgrok_policyd.so.*" >/dev/null; then
  SO=$(ls -1 "$ROOT/build"/libgrok_policyd.so.* | head -1)
fi
if [[ -L "${SO:-}" ]]; then
  target=$(readlink "$SO")
  if [[ "$target" != /* ]]; then
    SO="$(dirname "$SO")/$target"
  else
    SO="$target"
  fi
fi
test -n "${SO:-}" && test -f "$SO" || { echo "missing shared lib under $ROOT/build (make lib first)"; exit 1; }
echo "checking $SO"
need=$(readelf -d "$SO" 2>/dev/null | awk '/NEEDED/ {print $5}' | tr -d '[]' || true)
echo "DT_NEEDED: $need"
bad=
for lib in libnng libuv libsystemd libcap; do
  if echo "$need" | grep -q "$lib"; then
    echo "error: shared TCB lib must not NEEDED $lib (CLI-only)" >&2
    bad=1
  fi
done
# pkg-config must not list them either
pc="$ROOT/build/grok-policyd.pc"
if [[ -f "$pc" ]]; then
  libs=$(grep '^Libs:' "$pc" || true)
  echo "$libs"
  for lib in nng uv systemd cap; do
    if echo "$libs" | grep -q -- "-l$lib"; then
      echo "error: .pc Libs pulls -l$lib (CLI-only)" >&2
      bad=1
    fi
  done
fi
[[ -z "${bad:-}" ]] || exit 1
echo "ok: shared lib and .pc free of CLI host stack"
