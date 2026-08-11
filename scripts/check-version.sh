#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Assert public header / docs match VERSION + API_VERSION (single source).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
ver="$(tr -d '[:space:]' < VERSION)"
api="$(tr -d '[:space:]' < API_VERSION)"
IFS=. read -r major minor patch <<<"$ver"
hdr="include/grok-policyd/supervisor.h"
fail=0
check() {
  local pat="$1" file="$2" label="$3"
  if ! grep -Eq "$pat" "$file"; then
    echo "error: $label mismatch in $file (want matching $pat)" >&2
    fail=1
  fi
}
check "#define GROK_POLICYD_VERSION \"$ver\"" "$hdr" "VERSION string"
check "#define GROK_POLICYD_VERSION_MAJOR $major" "$hdr" "VERSION_MAJOR"
check "#define GROK_POLICYD_VERSION_MINOR $minor" "$hdr" "VERSION_MINOR"
check "#define GROK_POLICYD_VERSION_PATCH $patch" "$hdr" "VERSION_PATCH"
check "#define GROK_POLICYD_API_VERSION $api" "$hdr" "API_VERSION"
if [[ -f docs/source/conf.py ]]; then
  check "^release = \"$ver\"" docs/source/conf.py "sphinx release"
fi
if [[ -f docs/Doxyfile ]]; then
  check "PROJECT_NUMBER[[:space:]]*=[[:space:]]*\"$ver\"" docs/Doxyfile "doxygen PROJECT_NUMBER"
fi
if [[ -f pixi.toml ]]; then
  check "^version = \"$ver\"" pixi.toml "pixi workspace version"
fi
if [[ "$fail" -ne 0 ]]; then
  echo "hint: run ./scripts/sync-version.sh after editing VERSION or API_VERSION" >&2
  exit 1
fi
echo "ok: version $ver api $api consistent"
