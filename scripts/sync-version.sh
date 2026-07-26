#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Write package version macros into the public header from VERSION + API_VERSION.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
ver="$(tr -d '[:space:]' < VERSION)"
api="$(tr -d '[:space:]' < API_VERSION)"
IFS=. read -r major minor patch <<<"$ver"
hdr="include/grok-policyd/supervisor.h"
tmp="$(mktemp)"
# shellcheck disable=SC2016
sed -E \
  -e "s/#define GROK_POLICYD_VERSION \"[^\"]+\"/#define GROK_POLICYD_VERSION \"$ver\"/" \
  -e "s/#define GROK_POLICYD_VERSION_MAJOR [0-9]+/#define GROK_POLICYD_VERSION_MAJOR $major/" \
  -e "s/#define GROK_POLICYD_VERSION_MINOR [0-9]+/#define GROK_POLICYD_VERSION_MINOR $minor/" \
  -e "s/#define GROK_POLICYD_VERSION_PATCH [0-9]+/#define GROK_POLICYD_VERSION_PATCH $patch/" \
  -e "s/#define GROK_POLICYD_API_VERSION [0-9]+/#define GROK_POLICYD_API_VERSION $api/" \
  "$hdr" >"$tmp"
mv "$tmp" "$hdr"
# Keep Sphinx/Doxygen project numbers in sync (cosmetic)
if [[ -f docs/source/conf.py ]]; then
  sed -i -E "s/^release = \"[^\"]+\"/release = \"$ver\"/" docs/source/conf.py
fi
if [[ -f docs/Doxyfile ]]; then
  sed -i -E "s/^PROJECT_NUMBER[[:space:]]*=.*/PROJECT_NUMBER         = \"$ver\"/" docs/Doxyfile
fi
if [[ -f pixi.toml ]]; then
  sed -i -E "s/^version = \"[^\"]+\"/version = \"$ver\"/" pixi.toml
fi
echo "synced version $ver (api $api) → header + docs + pixi.toml"
