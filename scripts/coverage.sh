#!/usr/bin/env bash
# Instrumented Meson build + gcovr report (separate from default `build/`).
# https://mesonbuild.com/howtox.html#producing-a-coverage-report
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
B="${POLICYD_COVERAGE_BUILD_DIR:-build-cov}"
OUT="${POLICYD_COVERAGE_OUT_DIR:-coverage-out}"

command -v gcovr >/dev/null 2>&1 || {
  echo "error: gcovr required" >&2
  exit 1
}

rm -rf "$B" "$OUT"
meson setup "$B" -Db_coverage=true -Dbuildtype=debug
meson compile -C "$B"
export POLICYD_BUILD_DIR="$ROOT/$B"
export POLICYD_BIN="$ROOT/$B/phronesis"
meson test -C "$B" --print-errorlogs

mkdir -p "$OUT"
GCOV=gcov
if command -v x86_64-conda-linux-gnu-gcov >/dev/null 2>&1; then
  GCOV=x86_64-conda-linux-gnu-gcov
fi

# Invoke gcovr directly (Meson's coverage-text target is brittle with gcovr 8 + conda gcov).
# third_party/janet is linked into the coverage build (libjanet_amalg) and can
# emit .gcda gcov cannot resolve (corrupted / no_working_dir). Exclude those
# object dirs and ignore that gcov error class so report still covers src/.
gcovr \
  --gcov-executable "$GCOV" \
  --root "$ROOT" \
  --filter 'src/' \
  --exclude 'third_party/' \
  --exclude-directories '.*libjanet_amalg.*' \
  --exclude-directories '.*subprojects.*' \
  --gcov-ignore-errors=no_working_dir_found \
  --txt "$OUT/coverage.txt" \
  --xml "$OUT/coverage.xml" \
  --print-summary \
  "$B"

echo
cat "$OUT/coverage.txt"
echo
echo "ok: $OUT/coverage.xml"
