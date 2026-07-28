#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Generate c-capnproto C from schema/policy.capnp into Meson output paths.
# Usage: gen-capnp-c.sh INPUT.capnp OUT.c OUT.h CAPNPC_C_PATH
set -euo pipefail
inp=$1
out_c=$2
out_h=$3
capnpc_c=$4
outdir=$(dirname -- "$out_c")
mkdir -p "$outdir"
cp -f -- "$inp" "$outdir/policy.capnp"
command -v capnp >/dev/null || { echo "capnp not found" >&2; exit 1; }
test -x "$capnpc_c" || command -v "$capnpc_c" >/dev/null || {
	echo "capnpc-c not found: $capnpc_c" >&2
	exit 1
}
(cd "$outdir" && capnp compile -o"$capnpc_c" policy.capnp)
test -f "$outdir/policy.capnp.c"
test -f "$outdir/policy.capnp.h"
# Meson @OUTPUT@ may be the same paths; copy if names ever diverge.
if [[ "$(realpath "$out_c")" != "$(realpath "$outdir/policy.capnp.c")" ]]; then
	cp -f -- "$outdir/policy.capnp.c" "$out_c"
fi
if [[ "$(realpath "$out_h")" != "$(realpath "$outdir/policy.capnp.h")" ]]; then
	cp -f -- "$outdir/policy.capnp.h" "$out_h"
fi
