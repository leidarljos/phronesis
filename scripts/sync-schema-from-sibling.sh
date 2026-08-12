#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copy policy.capnp + util.capnp from another schema directory.
# This package's public interface is schema/; this script is optional
# refresh, not a build step. Pass the directory that contains both files.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
src="${1:-}"
if [[ -z "$src" ]]; then
  echo "usage: $0 /path/to/schema-dir" >&2
  exit 2
fi
if [[ ! -f "$src/policy.capnp" || ! -f "$src/util.capnp" ]]; then
  echo "sync-schema: missing policy.capnp/util.capnp under $src" >&2
  exit 1
fi
cp -f -- "$src/policy.capnp" "$ROOT/schema/policy.capnp"
cp -f -- "$src/util.capnp" "$ROOT/schema/util.capnp"
echo "sync-schema: copied policy.capnp + util.capnp from $src"
