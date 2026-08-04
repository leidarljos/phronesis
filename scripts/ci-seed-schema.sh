#!/usr/bin/env bash
# Pre-seed subprojects/grokos-schema for Meson subproject (CI job-token).
set -euo pipefail
dest="${CI_PROJECT_DIR}/subprojects/grokos-schema"
if [[ -f "${dest}/schema/policy.capnp" && -f "${dest}/meson.build" ]]; then
  echo "schema SoT already present at ${dest}"
  git -C "${dest}" rev-parse --short HEAD || true
  exit 0
fi
: "${CI_JOB_TOKEN:?CI_JOB_TOKEN required}"
: "${CI_SERVER_HOST:?CI_SERVER_HOST required}"
url="https://gitlab-ci-token:${CI_JOB_TOKEN}@${CI_SERVER_HOST}/trace-analysis/grokos-packages/grokos-schema.git"
rm -rf "${dest}"
mkdir -p "${CI_PROJECT_DIR}/subprojects"
rev=""
if [[ -f subprojects/grokos-schema.wrap ]]; then
  rev=$(grep -E '^revision' subprojects/grokos-schema.wrap | awk '{print $3}' | tr -d '[:space:]' || true)
fi
git clone --filter=blob:none "$url" "${dest}"
if [[ -n "${rev}" ]]; then
  git -C "${dest}" fetch --depth 1 origin "${rev}" 2>/dev/null || true
  git -C "${dest}" checkout --detach "${rev}" 2>/dev/null \
    || git -C "${dest}" checkout --detach "origin/${rev}" 2>/dev/null \
    || git -C "${dest}" checkout feat/meson-schema-project 2>/dev/null \
    || git -C "${dest}" checkout main
else
  git -C "${dest}" checkout feat/meson-schema-project 2>/dev/null \
    || git -C "${dest}" checkout main
fi
test -f "${dest}/schema/policy.capnp"
test -f "${dest}/meson.build"
echo "GROKOS_SCHEMA_SOT=${dest} sha=$(git -C "${dest}" rev-parse HEAD)"
