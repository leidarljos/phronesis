#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Generate c-capnproto C from schemadir (policy + util) into Meson OUTDIR.
# Both .capnp files must live in SCHEMA_DIR (default: repo schema/); no
# silent mix of policy with a different util.
# Usage: gen-capnp-c.sh SCHEMA_DIR OUTDIR CAPNPC_C
set -euo pipefail
schema_dir=$1
outdir=$2
capnpc_c=$3
mkdir -p "$outdir"
for f in util.capnp policy.capnp; do
	if [[ ! -f "$schema_dir/$f" ]]; then
		echo "missing $schema_dir/$f (Cap'n SoT schemadir incomplete)" >&2
		exit 1
	fi
	cp -f -- "$schema_dir/$f" "$outdir/$f"
done
command -v capnp >/dev/null || { echo "capnp not found" >&2; exit 1; }
test -x "$capnpc_c" || command -v "$capnpc_c" >/dev/null || {
	echo "capnpc-c not found: $capnpc_c" >&2
	exit 1
}
# util first (AgentId/TraceId), then policy (imports util).
(cd "$outdir" && capnp compile -I. -o"$capnpc_c" util.capnp)
(cd "$outdir" && capnp compile -I. -o"$capnpc_c" policy.capnp)
test -f "$outdir/util.capnp.c" && test -f "$outdir/util.capnp.h"
test -f "$outdir/policy.capnp.c" && test -f "$outdir/policy.capnp.h"
test -f "$outdir/util.capnp" && test -f "$outdir/policy.capnp"
