#!/usr/bin/env bash
# Prove the pixi env can satisfy meson wire deps before setup/compile.
set -euo pipefail
echo "=== grok-policyd pixi env ==="
: "${CONDA_PREFIX:?run via pixi}"

echo "CONDA_PREFIX=${CONDA_PREFIX}"
export PKG_CONFIG_PATH="${CONDA_PREFIX}/lib/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"

need_pc() {
  local name=$1
  if pkg-config --exists "${name}"; then
    echo "pkg-config ${name}=$(pkg-config --modversion "${name}")"
  else
    echo "FATAL: pkg-config --exists ${name} failed (need ${name}.pc under ${CONDA_PREFIX}/lib/pkgconfig)" >&2
    exit 1
  fi
}

need_pc c-capnproto
need_pc cmocka
need_pc libsystemd

# nng: conda-forge frequently omits nng.pc; meson falls back to find_library.
if pkg-config --exists nng; then
  echo "pkg-config nng=$(pkg-config --modversion nng)"
else
  echo "pkg-config nng=missing (ok; meson uses find_library('nng'))"
fi
test -r "${CONDA_PREFIX}/include/nng/nng.h" || {
  echo "FATAL: missing ${CONDA_PREFIX}/include/nng/nng.h" >&2
  exit 1
}
# Shared or static; any of these means the linker can resolve -lnng under CONDA_PREFIX.
nng_lib=
for cand in \
  "${CONDA_PREFIX}/lib/libnng.so" \
  "${CONDA_PREFIX}/lib/libnng.a" \
  "${CONDA_PREFIX}/lib64/libnng.so" \
  "${CONDA_PREFIX}/lib64/libnng.a"
do
  if [[ -e "${cand}" ]]; then
    nng_lib=${cand}
    break
  fi
done
if [[ -z "${nng_lib}" ]]; then
  echo "FATAL: no libnng under ${CONDA_PREFIX}/lib (headers alone are not enough for meson)" >&2
  ls -la "${CONDA_PREFIX}/lib"/libnng* 2>/dev/null || true
  exit 1
fi
echo "nng header+lib ok (${nng_lib})"

test -r "${CONDA_PREFIX}/include/systemd/sd-daemon.h" \
  || test -r "${CONDA_PREFIX}/include/systemd/sd-event.h" || {
  echo "FATAL: missing systemd headers under ${CONDA_PREFIX}/include/systemd" >&2
  exit 1
}
command -v capnpc-c >/dev/null || {
  echo "FATAL: capnpc-c not on PATH" >&2
  exit 1
}
command -v capnp >/dev/null || {
  echo "FATAL: capnp not on PATH" >&2
  exit 1
}
command -v meson >/dev/null
command -v ninja >/dev/null
echo "c-capnproto+cmocka+libsystemd+nng+tools=ok"
