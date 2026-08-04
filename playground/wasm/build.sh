#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Direct-emcc playground build (v1). Produces:
#   playground/dist-wasm/policyd-playground.js
#   playground/dist-wasm/policyd-playground.wasm
#
# Product meson default path is untouched. Requires:
#   - emcc on PATH (emsdk activate, or conda-forge emscripten)
#   - host capnp + capnpc-c (product pixi env) for schema codegen
#   - network once to fetch HaoZeke/c-capnproto sources into wasm/deps/
#
# Usage (on rg.terra recommended):
#   pixi install --locked
#   eval "$(pixi shell-hook)"   # host capnp tools
#   source ~/emsdk/emsdk_env.sh # or: pixi run -e playground ...
#   bash playground/wasm/build.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

WASM_DIR="$ROOT/playground/wasm"
DIST="$ROOT/playground/dist-wasm"
GEN="$WASM_DIR/gen"
DEPS="$WASM_DIR/deps"
MEMFS="$WASM_DIR/memfs"
CAPN_SRC="${CAPN_C_SRC:-$DEPS/c-capnproto}"
CAPN_TAG="${CAPN_C_TAG:-v0.1.0}"

need() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required tool: $1" >&2
		exit 1
	}
}

need emcc
need capnp
need bash

CAPNPC_C="${CAPNPC_C:-}"
if [[ -z "$CAPNPC_C" ]]; then
	if command -v capnpc-c >/dev/null 2>&1; then
		CAPNPC_C=$(command -v capnpc-c)
	else
		echo "error: capnpc-c not found (install c-capnproto / product pixi env)" >&2
		exit 1
	fi
fi

mkdir -p "$DIST" "$GEN" "$DEPS"

# --- c-capnproto pure-C runtime sources (compile with emcc; not product vendor) ---
if [[ ! -f "$CAPN_SRC/lib/capn.c" ]]; then
	echo "fetching c-capnproto $CAPN_TAG into $CAPN_SRC" >&2
	rm -rf "$CAPN_SRC"
	git clone --depth 1 --branch "$CAPN_TAG" \
		https://github.com/HaoZeke/c-capnproto.git "$CAPN_SRC"
fi

# --- schema C (host capnpc-c) ---
# gen-capnp-c.sh SCHEMA_DIR OUTDIR CAPNPC_C (SoT pin or subproject schemadir)
SCHEMA_DIR="${GROKOS_SCHEMA_DIR:-$ROOT/schema}"
if [[ ! -f "$SCHEMA_DIR/policy.capnp" && -f "$ROOT/subprojects/grokos-schema/schema/policy.capnp" ]]; then
	SCHEMA_DIR="$ROOT/subprojects/grokos-schema/schema"
fi
echo "generating Cap'n C schema from $SCHEMA_DIR into $GEN" >&2
bash "$ROOT/scripts/gen-capnp-c.sh" \
	"$SCHEMA_DIR" \
	"$GEN" \
	"$CAPNPC_C"

# --- MEMFS seed ---
bash "$WASM_DIR/memfs_seed.sh" "$MEMFS"

# --- host golden ShellCheck fixtures ---
FIXTURE="$ROOT/playground/fixtures/shell/shell_check_uv_run.bin"
FIXTURE_CURL="$ROOT/playground/fixtures/shell/shell_check_curl_sh.bin"
need_fixture_export=0
if [[ ! -f "$FIXTURE" || ! -f "$FIXTURE_CURL" ]]; then
	need_fixture_export=1
fi
if [[ "$need_fixture_export" -eq 1 ]]; then
	if command -v cc >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1; then
		HOSTCC=$(command -v cc 2>/dev/null || command -v gcc)
		echo "building ShellCheck export helper with $HOSTCC" >&2
		"$HOSTCC" -std=c11 -O2 \
			-I"$GEN" -I"$ROOT/include" -I"$CAPN_SRC/lib" \
			"$ROOT/playground/scripts/export_shell_msg.c" \
			"$GEN/policy.capnp.c" \
			"$GEN/util.capnp.c" \
			"$CAPN_SRC/lib/capn.c" \
			"$CAPN_SRC/lib/capn-malloc.c" \
			"$CAPN_SRC/lib/capn-stream.c" \
			-o "$GEN/export_shell_msg"
		if [[ ! -f "$FIXTURE" ]]; then
			"$GEN/export_shell_msg" "$FIXTURE"
		else
			echo "using existing fixture $FIXTURE" >&2
		fi
		if [[ ! -f "$FIXTURE_CURL" ]]; then
			"$GEN/export_shell_msg" --curl-sh "$FIXTURE_CURL"
		else
			echo "using existing fixture $FIXTURE_CURL" >&2
		fi
	else
		echo "error: missing ShellCheck fixture(s) and no host cc to generate" >&2
		exit 1
	fi
else
	echo "using existing fixtures $FIXTURE and $FIXTURE_CURL" >&2
fi

# --- compile + link ---
# Product TCB units + capnp-janet pure C + janet amalg + embind_api.
# fork/start is not exercised by smoke; agent workspace comes from MEMFS slot.
# Playground always enables TRACE ring (product meson never defines it).

INC=(
	-I"$ROOT/include"
	-I"$ROOT/src"
	-I"$GEN"
	-I"$ROOT/third_party/janet"
	-I"$ROOT/subprojects/capnp-janet/include"
	-I"$CAPN_SRC/lib"
)

# janet.h already forces NO_DYNAMIC_MODULES/NO_PROCESSES under __EMSCRIPTEN__.
COMMON_CFLAGS=(
	-std=gnu11
	-O2
	-D_GNU_SOURCE
	-DGROKOS_POLICYD_PLAYGROUND=1
	-DGROKOS_POLICYD_TRACE=1
	-DJANET_API=
	-DJANET_SPAWN_NO_CHDIR
	-DJANET_NO_EV
	-DJANET_NO_NET
	-Wno-unused-parameter
	-Wno-sign-compare
	"${INC[@]}"
)

SRCS=(
	"$ROOT/src/paths.c"
	"$ROOT/src/action_log.c"
	"$ROOT/src/cgroup.c"
	"$ROOT/src/policy.c"
	"$ROOT/src/policy_reason.c"
	"$ROOT/src/policy_janet.c"
	"$ROOT/src/policy_trace.c"
	"$ROOT/src/supervisor.c"
	"$ROOT/src/version.c"
	"$ROOT/src/capnp_api.c"
	"$GEN/policy.capnp.c"
	"$GEN/util.capnp.c"
	"$ROOT/subprojects/capnp-janet/src/capnp_message.c"
	"$ROOT/subprojects/capnp-janet/src/capnp_builder.c"
	"$ROOT/subprojects/capnp-janet/src/janet_mod.c"
	"$ROOT/third_party/janet/janet.c"
	"$CAPN_SRC/lib/capn.c"
	"$CAPN_SRC/lib/capn-malloc.c"
	"$CAPN_SRC/lib/capn-stream.c"
	"$WASM_DIR/embind_api.c"
)

# Ensure capnp-janet subproject sources exist (meson wrap / shallow clone).
if [[ ! -f "$ROOT/subprojects/capnp-janet/src/janet_mod.c" ]]; then
	echo "fetching capnp-janet into subprojects/" >&2
	git clone --depth 1 \
		https://github.com/HaoZeke/capnp-janet.git \
		"$ROOT/subprojects/capnp-janet"
fi

EXPORTS='["_pd_supervisor_open","_pd_supervisor_close","_pd_check_shell","_pd_check_path","_pd_check_seat","_pd_check_risk","_pd_reload_shell_pack","_pd_reload_pack_path","_pd_read_decision","_pd_clear_trace","_pd_take_trace_json","_pd_free","_malloc","_free"]'
RUNTIME='["ccall","cwrap","getValue","setValue","UTF8ToString","stringToUTF8","HEAPU8","FS"]'

echo "emcc → $DIST/policyd-playground.js" >&2
emcc "${COMMON_CFLAGS[@]}" \
	"${SRCS[@]}" \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=PolicydPlayground \
	-s EXPORT_ES6=1 \
	-s ENVIRONMENT=web,node \
	-s EXPORTED_FUNCTIONS="$EXPORTS" \
	-s EXPORTED_RUNTIME_METHODS="$RUNTIME" \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s INITIAL_MEMORY=67108864 \
	-s STACK_SIZE=1048576 \
	-s FILESYSTEM=1 \
	-s FORCE_FILESYSTEM=1 \
	-s ERROR_ON_UNDEFINED_SYMBOLS=1 \
	--preload-file "$MEMFS@/" \
	-o "$DIST/policyd-playground.js"

test -f "$DIST/policyd-playground.js"
test -f "$DIST/policyd-playground.wasm"
# .data accompanies --preload-file; required for MEMFS seed
test -f "$DIST/policyd-playground.data"

# Strip absolute host paths emcc embeds (PACKAGE_NAME / datafile keys).
# Keeps tree free of personal home dirs for leakguard + portable Pages assets.
python3 - "$DIST" "$ROOT" <<'PY'
import pathlib, re, sys
dist = pathlib.Path(sys.argv[1])
root = pathlib.Path(sys.argv[2]).resolve()
js = dist / "policyd-playground.js"
text = js.read_text(errors="replace")
# Prefer longest-prefix rewrites first.
for abs_p in (str(dist.resolve()), str(root)):
    text = text.replace(abs_p + "/", "")
    text = text.replace(abs_p, ".")
# Drop residual /home/<user>/... absolute segments (keep emscripten /home/web_user).
text = re.sub(r"/home/(?!web_user)[A-Za-z0-9_.-]+/[^\s\"']+", ".", text)
js.write_text(text)
print(f"scrubbed host paths in {js}", file=sys.stderr)
PY

echo "OK: $DIST/policyd-playground.{js,wasm,data}" >&2
ls -la "$DIST"/policyd-playground.*
