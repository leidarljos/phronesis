#!/usr/bin/env bash
set -euo pipefail
echo "=== grok-policyd pixi env ==="
: "${CONDA_PREFIX:?run via pixi}"
echo "CONDA_PREFIX=${CONDA_PREFIX}"
command -v meson >/dev/null
command -v ninja >/dev/null
pkg-config --exists cmocka && echo "cmocka=$(pkg-config --modversion cmocka)"
test -f include/grok-policyd/supervisor.h
echo "tcb-library-only=ok (no nng serve)"
