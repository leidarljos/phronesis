#!/usr/bin/env bash
# Refresh vendor/ from the repository and the pinned C dependencies.
#
#   scripts/vendor-refresh.sh [--wraps DIR] [--capnpc-c PATH]
#
# --wraps DIR      checkouts of c-capnproto and capnp-janet at the revisions
#                  subprojects/*.wrap pin; cloned into a temporary directory
#                  when not given.
# --capnpc-c PATH  the Cap'n Proto C generator; built from the c-capnproto
#                  checkout with cc when not given.
#
# What lands in vendor/ is what build.rs compiles: the phronesis library
# sources and header, the Janet amalgamation, the c-capnproto runtime, the
# capnp-janet runtime, and the C generated from schema/*.capnp. policy/ is
# the pack as it is, no rule changes ride along. vendor/PINS records the
# revisions.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
repo=$(cd "$here/../.." && pwd)
wraps=""
capnpc_c=""
while [ $# -gt 0 ]; do
  case $1 in
    --wraps) wraps=$2; shift 2 ;;
    --capnpc-c) capnpc_c=$2; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
wrap_rev() { sed -n 's/^revision = //p' "$repo/subprojects/$1.wrap"; }
wrap_url() { sed -n 's/^url = //p' "$repo/subprojects/$1.wrap"; }
if [ -z "$wraps" ]; then
  wraps=$(mktemp -d "${TMPDIR:-/tmp}/phronesis-wraps.XXXXXX")
  for w in c-capnproto capnp-janet; do
    git clone -q "$(wrap_url $w)" "$wraps/$w"
    git -C "$wraps/$w" checkout -q "$(wrap_rev $w)"
  done
fi
for w in c-capnproto capnp-janet; do
  have=$(git -C "$wraps/$w" rev-parse HEAD)
  want=$(wrap_rev $w)
  [ "$have" = "$want" ] || { echo "$w is at $have, the wrap pins $want" >&2; exit 1; }
done
if [ -z "$capnpc_c" ]; then
  capnpc_c=$wraps/capnpc-c
  (cd "$wraps/c-capnproto" && cc -O1 -w -Ilib -Icompiler \
    compiler/capnpc-c.c compiler/codecgen.c compiler/schema.capnp.c compiler/str.c \
    lib/capn.c lib/capn-malloc.c lib/capn-stream.c -o "$capnpc_c")
fi
v=$here/vendor
rm -rf "$v" "$here/policy"
mkdir -p "$v/phronesis/src" "$v/phronesis/include/phronesis" "$v/janet" "$v/c-capnproto" \
  "$v/capnp-janet/src" "$v/capnp-janet/include" "$v/capnp" "$here/policy/lib"
# The library sources: not the CLI (phronesis.c) and not the image tool.
for f in "$repo"/src/*.c "$repo"/src/*.h; do
  case $(basename "$f") in phronesis.c|pack_image_main.c) continue ;; esac
  cp "$f" "$v/phronesis/src/"
done
cp "$repo/include/phronesis/supervisor.h" "$v/phronesis/include/phronesis/"
cp "$repo/third_party/janet/janet.c" "$repo/third_party/janet/janet.h" "$v/janet/"
cp "$repo/third_party/NOTICE" "$v/janet/NOTICE"
cp "$wraps"/c-capnproto/lib/{capn.c,capn-malloc.c,capn-stream.c,capnp_c.h,capnp_priv.h,capn-list.inc} "$v/c-capnproto/"
cp "$wraps/c-capnproto/COPYING" "$v/c-capnproto/COPYING"
cp "$wraps"/capnp-janet/src/*.c "$v/capnp-janet/src/"
cp -r "$wraps/capnp-janet/include/capnp-janet" "$v/capnp-janet/include/"
cp "$wraps/capnp-janet/LICENSE" "$v/capnp-janet/LICENSE"
bash "$repo/scripts/gen-capnp-c.sh" "$repo/schema" "$v/capnp" "$capnpc_c"
cp "$repo/schema/SCHEMA_PIN" "$v/capnp/SCHEMA_PIN"
cp "$repo/policy/shell.janet" "$repo/policy/seat.janet" "$here/policy/"
cp "$repo"/policy/lib/*.janet "$here/policy/lib/"
{
  echo "phronesis $(git -C "$repo" rev-parse HEAD) $(tr -d '[:space:]' < "$repo/VERSION")"
  echo "c-capnproto $(wrap_rev c-capnproto)"
  echo "capnp-janet $(wrap_rev capnp-janet)"
  echo "schema $(sed -n "s/^interface_version=//p" "$repo/schema/SCHEMA_PIN")"
} > "$v/PINS"
echo "vendored into $v; pack into $here/policy"
