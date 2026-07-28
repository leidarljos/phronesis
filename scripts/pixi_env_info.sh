#!/usr/bin/env bash
set -euo pipefail
echo "pixi: $(command -v pixi || true)"
pixi --version 2>/dev/null || true
echo "capnp: $(command -v capnp || true)"
echo "capnpc-c: $(command -v capnpc-c || true)"
echo "meson: $(command -v meson || true)"
