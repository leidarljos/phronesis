#!/usr/bin/env bash
# Thin wrapper: build this package then run session smoke if co-located.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
make -C "$ROOT" build/grok-policyd test-wire
export POLICYD_BIN="$ROOT/build/grok-policyd"
export POLICYD_ROOT="$ROOT"
SES="${SES_ROOT:-$ROOT/../grokos-session}"
if [[ -x "$SES/scripts/smoke_policyd_wire.sh" ]]; then
  exec "$SES/scripts/smoke_policyd_wire.sh"
fi
echo "error: session smoke script not found at $SES/scripts/smoke_policyd_wire.sh" >&2
echo "hint: set SES_ROOT to grokos-session checkout" >&2
exit 1
