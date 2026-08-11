#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Seed playground/wasm/memfs for --preload-file (Emscripten MEMFS root).
#
# Multi-pack layout (matches product GROKOS_POLICYD_JANET_PACK colon / packs.d):
#   /policy/shell.janet     product entry (shell-check + audio-check)
#   /policy/lib/*.janet     pure helpers for the entry
#   /policy/packs.d/*.janet optional extra packs (compose deny > prompt > allow)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MEMFS="${1:-$ROOT/playground/wasm/memfs}"
AGENT_ID="00000000000000010000000000000002"

rm -rf "$MEMFS"
mkdir -p \
	"$MEMFS/policy/lib" \
	"$MEMFS/policy/packs.d" \
	"$MEMFS/pd-state/log" \
	"$MEMFS/pd-state/policyd" \
	"$MEMFS/pd-runtime/agents" \
	"$MEMFS/ws"

# Product pack + pure helpers.
cp -f "$ROOT/policy/shell.janet" "$MEMFS/policy/shell.janet"
cp -f "$ROOT/policy/lib/"*.janet "$MEMFS/policy/lib/"

# Optional second pack: deny argv containing the multipack-demo canary token.
# Loaded via colon list or packs.d directory so Author mode can exercise
# multi-pack composition without replacing product shell.janet.
cat >"$MEMFS/policy/packs.d/extra-canary.janet" <<'JANET'
# Playground multipack demo pack (not product).
# Cap'n ShellView in → Cap'n PolicyDecision out (same contract as product packs).
# Deny when any argv element contains "multipack-demo"; otherwise allow so
# product shell.janet remains the interesting path for normal fixtures.

(defn- decide [decision code reason]
  (capnp/build-message 1 2
                       @[[:u16 0 decision]
                         [:u16 2 code]
                         [:text 0 reason]]))

(defn- read-argv [root]
  (def lp (capnp/getp root shell-view-argv-ptr))
  (def n (capnp/list-len lp))
  (def out @[])
  (var i 0)
  (while (< i n)
    (array/push out (capnp/list-get-text lp i))
    (set i (+ i 1)))
  out)

(defn shell-check
  ``Extra multipack pack: deny argv containing multipack-demo.``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (def argv (read-argv root))
  (var hit false)
  (each a argv
    (when (and (string? a) (string/find "multipack-demo" a))
      (set hit true)))
  (if hit
    (decide Decision-deny PolicyReason-shellDangerousRunner
            "multipack-demo canary")
    (decide Decision-allow PolicyReason-shellExecAllow
            "multipack extra allow")))
JANET

# Workspace PEP 723 script for checkShell smoke (uv run --script ok.py).
cat >"$MEMFS/ws/ok.py" <<'PY'
# /// script
# requires-python = ">=3.11"
# ///
print(1)
PY

cat >"$MEMFS/ws/bare.py" <<'PY'
print(1)
PY

printf '%s 0 0 0 -1 agent /ws\n' "$AGENT_ID" \
	>"$MEMFS/pd-runtime/agents/${AGENT_ID}.slot"

: >"$MEMFS/pd-state/log/actions.jsonl"

echo "memfs seeded at $MEMFS (multi-pack: shell.janet + packs.d/)" >&2
