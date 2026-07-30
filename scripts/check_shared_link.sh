#!/usr/bin/env bash
# Product link-line contract for libgrok_policyd.so (not an inventory dump).
#
# Embedders open this shared object and call grok_policyd_handle_capnp. The
# .so must:
#   - export that entry
#   - NEEDED the Cap'n pure-C runtime (libcapnp_c)
#   - not NEEDED a peer/socket host stack (nng, libuv, systemd, libcap)
#
# Visibility of other symbols is compile-time (gnu_symbol_visibility=hidden
# + GROK_POLICYD_API). This script does not re-list internals.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${POLICYD_BUILD_DIR:-$ROOT/build}"

SO=
if [[ -e "$BUILD/libgrok_policyd.so" ]]; then
	SO="$BUILD/libgrok_policyd.so"
elif compgen -G "$BUILD/libgrok_policyd.so.*" >/dev/null; then
	SO=$(ls -1 "$BUILD"/libgrok_policyd.so.* | head -1)
fi
if [[ -L "${SO:-}" ]]; then
	target=$(readlink "$SO")
	if [[ "$target" != /* ]]; then
		SO="$(dirname "$SO")/$target"
	else
		SO="$target"
	fi
fi
if [[ -z "${SO:-}" || ! -f "$SO" ]]; then
	echo "error: no libgrok_policyd shared library under $BUILD" >&2
	exit 1
fi

PATH="${PATH:-/usr/bin:/bin}:/usr/bin:/bin"

mapfile -t needed < <(
	readelf -d "$SO" 2>/dev/null | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' || true
)
if [[ ${#needed[@]} -eq 0 ]]; then
	mapfile -t needed < <(
		readelf -d "$SO" 2>/dev/null | tr -d '[]' | awk '$2 == "(NEEDED)" { print $NF }' || true
	)
fi

have_capnp=
for soname in "${needed[@]}"; do
	case "$soname" in
	libnng.so* | libuv.so* | libsystemd.so* | libcap.so.*)
		echo "error: shared lib must not NEEDED $soname (in-process Cap'n FFI, not a peer daemon)" >&2
		exit 1
		;;
	libcapnp_c.so*)
		have_capnp=1
		;;
	esac
done
if [[ -z "$have_capnp" ]]; then
	echo "error: shared lib must NEEDED libcapnp_c.so (c-capnproto)" >&2
	exit 1
fi

# Dynamic export of the product Cap'n entry (default-visibility).
syms=$(nm -D --defined-only "$SO" 2>/dev/null || nm -D "$SO" 2>/dev/null || true)
case "$syms" in
*" T grok_policyd_handle_capnp"* | *" D grok_policyd_handle_capnp"* | *" T _grok_policyd_handle_capnp"*)
	;;
*)
	echo "error: grok_policyd_handle_capnp not exported from $SO" >&2
	exit 1
	;;
esac

echo "ok: $SO — handle_capnp exported, libcapnp_c NEEDED, no peer host stack"
