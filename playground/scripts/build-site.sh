#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Build the Astro playground site (no emcc).
# Requires: node/npm, optional playground/dist-wasm for evaluator assets.
#
# Usage:
#   bash playground/scripts/build-site.sh
#   PUBLIC_BASE=/ bash playground/scripts/build-site.sh   # root-relative
#   PUBLIC_BASE=/grok-policyd/ bash playground/scripts/build-site.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ASTRO="$ROOT/playground/astro"
export PUBLIC_BASE="${PUBLIC_BASE:-/grok-policyd/}"

cd "$ASTRO"
if [[ ! -d node_modules ]]; then
	npm ci
else
	# Prefer lockfile install when present
	if [[ -f package-lock.json ]]; then
		npm ci
	fi
fi

node "$ROOT/playground/scripts/copy-wasm-assets.mjs"
python3 "$ROOT/playground/scripts/gen-encyclopedia.py"
npm run build

echo "OK: site → $ASTRO/dist (base=$PUBLIC_BASE)" >&2
ls -la "$ASTRO/dist" | head -20
