#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
echo "grok-policyd pixi env"
echo "  pwd=$(pwd)"
echo "  pixi=$(command -v pixi || true)"
pixi --version 2>/dev/null || true
echo "  cc=$(command -v cc || true)"
cc --version 2>/dev/null | head -1 || true
echo "  pkg-config=$(command -v pkg-config || true)"
pkg-config --modversion cmocka 2>/dev/null && echo "  cmocka ok" || echo "  cmocka MISSING"
echo "  make=$(command -v make || true)"
