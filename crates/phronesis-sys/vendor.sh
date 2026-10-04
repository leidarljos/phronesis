#!/usr/bin/env bash
# Fill vendor/ with every C source phronesis-sys compiles: the library's
# own sources, the Janet amalgamation, c-capnproto and capnp-janet at the
# revisions the meson wraps pin, the generated Cap'n Proto C for the
# schema, and the Janet policy pack. Run from anywhere; takes the generated
# C from a meson build dir when one is given, else runs capnp itself.
#
#   crates/phronesis-sys/vendor.sh [MESON_BUILD_DIR]
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/../.." && pwd)
vendor="$here/vendor"
build=${1:-}

wrap_field() { # file key
    sed -n "s/^$2 = //p" "$1" | head -1
}

fetch() { # wrap-file dest
    local url rev
    url=$(wrap_field "$1" url)
    rev=$(wrap_field "$1" revision)
    if [ -d "$2/.git" ] && [ "$(git -C "$2" rev-parse HEAD)" = "$rev" ]; then
        return
    fi
    rm -rf "$2"
    git clone -q "$url" "$2"
    git -C "$2" checkout -q "$rev"
}

rm -rf "$vendor"
mkdir -p "$vendor/phronesis/src" "$vendor/phronesis/include/phronesis" \
    "$vendor/janet" "$vendor/c-capnproto/lib" "$vendor/c-capnproto/compiler" \
    "$vendor/capnp-janet/src" "$vendor/capnp-janet/include" "$vendor/gen" \
    "$vendor/pack/policy/lib"

# The library, without the CLI's main.
for f in "$root"/src/*.c "$root"/src/*.h; do
    case "$(basename "$f")" in phronesis.c) ;; *) cp "$f" "$vendor/phronesis/src/";; esac
done
cp "$root"/include/phronesis/*.h "$vendor/phronesis/include/phronesis/"
cp "$root"/third_party/janet/janet.c "$root"/third_party/janet/janet.h "$vendor/janet/"
cp "$root"/third_party/janet/NOTICE "$vendor/janet/NOTICE"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fetch "$root/subprojects/c-capnproto.wrap" "$tmp/c-capnproto"
fetch "$root/subprojects/capnp-janet.wrap" "$tmp/capnp-janet"
# The library's sources as the pinned meson.build lists them: lib/*.c and
# the generated rpc schema C under compiler/. Headers all come along.
cp "$tmp"/c-capnproto/lib/*.c "$tmp"/c-capnproto/lib/*.h "$tmp"/c-capnproto/lib/*.inc "$vendor/c-capnproto/lib/"
sed -n "/^libcapnp_src = \[/,/^\]/p" "$tmp/c-capnproto/meson.build" \
    | sed -n "s/^ *'compiler' \/ '\([^']*\)',/\1/p" \
    | while read -r f; do cp "$tmp/c-capnproto/compiler/$f" "$vendor/c-capnproto/compiler/"; done
cp "$tmp"/c-capnproto/compiler/*.h "$vendor/c-capnproto/compiler/"
cp "$tmp"/c-capnproto/COPYING "$vendor/c-capnproto/"
cp "$tmp"/capnp-janet/src/*.c "$vendor/capnp-janet/src/"
cp -r "$tmp"/capnp-janet/include/capnp-janet "$vendor/capnp-janet/include/"
cp "$tmp"/capnp-janet/LICENSE "$vendor/capnp-janet/"

# Generated Cap'n Proto C for schema/{policy,util}.capnp.
if [ -n "$build" ] && [ -f "$build/policy.capnp.c" ]; then
    cp "$build"/{policy,util}.capnp.{c,h} "$vendor/gen/"
else
    capnpc_c=$(command -v capnpc-c || true)
    [ -n "$capnpc_c" ] || { echo "vendor.sh: no meson build dir given and capnpc-c not on PATH" >&2; exit 1; }
    bash "$root/scripts/gen-capnp-c.sh" "$root/schema" "$vendor/gen" "$capnpc_c"
    rm -f "$vendor"/gen/*.capnp
fi

# The policy pack, in the layout the loader trusts under a prefix.
cp "$root"/policy/*.janet "$vendor/pack/policy/"
cp "$root"/policy/lib/*.janet "$vendor/pack/policy/lib/"
cp "$root"/schema/SCHEMA_PIN "$vendor/pack/"

{
    echo "phronesis $(git -C "$root" rev-parse HEAD)"
    echo "c-capnproto $(wrap_field "$root/subprojects/c-capnproto.wrap" revision)"
    echo "capnp-janet $(wrap_field "$root/subprojects/capnp-janet.wrap" revision)"
} > "$vendor/PINS"
echo "vendored: $(find "$vendor" -type f | wc -l) files, $(cat "$vendor"/*/*.c "$vendor"/*/*/*.c 2>/dev/null | wc -l) lines of C"
