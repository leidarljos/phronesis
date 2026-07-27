#!/usr/bin/env bash
# SIGTERM must stop grok-policyd serve (host_should_stop polled in loop).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${POLICYD_BIN:-$ROOT/build/grok-policyd}"
test -x "$BIN" || { echo "missing $BIN"; exit 1; }
BASE=$(mktemp -d "${TMPDIR:-/var/tmp}/policyd-sig.XXXXXX")
STATE="$BASE/state"
RUN="$BASE/run"
SOCK="$BASE/policyd.sock"
mkdir -p "$STATE" "$RUN"
chmod 700 "$STATE" "$RUN"
"$BIN" --state-dir "$STATE" --runtime-dir "$RUN" serve --socket "$SOCK" &
PID=$!
cleanup() { kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; rm -rf "$BASE"; }
trap cleanup EXIT
for _ in $(seq 1 50); do
  [[ -e "$SOCK" ]] && break
  kill -0 "$PID" 2>/dev/null || { echo "serve died early"; exit 1; }
  sleep 0.05
done
[[ -e "$SOCK" ]] || { echo "sock not ready"; exit 1; }
kill -TERM "$PID"
# must exit within 3s (recv timeout 500ms + margin)
for _ in $(seq 1 30); do
  if ! kill -0 "$PID" 2>/dev/null; then
    echo "ok: serve exited on SIGTERM"
    wait "$PID" 2>/dev/null || true
    trap - EXIT
    rm -rf "$BASE"
    exit 0
  fi
  sleep 0.1
done
echo "error: serve ignored SIGTERM" >&2
exit 1
