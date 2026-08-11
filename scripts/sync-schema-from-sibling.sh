#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copy policy.capnp + util.capnp from a sibling grokos-schema tree.
# This package's public interface is schema/; this script is optional
# monorepo refresh, not a build step.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
src="${1:-$ROOT/../../grokos-schema/schema}"
if [[ ! -f "$src/policy.capnp" || ! -f "$src/util.capnp" ]]; then
  echo "sync-schema: missing policy.capnp/util.capnp under $src" >&2
  exit 1
fi
cp -f -- "$src/policy.capnp" "$ROOT/schema/policy.capnp"
cp -f -- "$src/util.capnp" "$ROOT/schema/util.capnp"
echo "sync-schema: copied policy.capnp + util.capnp from $src"
