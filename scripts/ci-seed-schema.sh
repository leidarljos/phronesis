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
if [[ -z "${rev}" ]]; then
  echo "ci-seed-schema: wrap missing revision" >&2
  exit 1
fi
git -C "${dest}" fetch origin "${rev}" \
  || git -C "${dest}" fetch --depth 1 origin "${rev}" \
  || true
if ! git -C "${dest}" checkout --detach "${rev}" 2>/dev/null \
  && ! git -C "${dest}" checkout --detach "origin/${rev}" 2>/dev/null; then
  echo "ci-seed-schema: cannot checkout wrap revision ${rev}" >&2
  exit 1
fi
got="$(git -C "${dest}" rev-parse HEAD)"
# wrap rev may be an annotated tag; compare peeled commit
want="$(git -C "${dest}" rev-parse "${rev}^{commit}" 2>/dev/null || echo "$rev")"
if [[ "$got" != "$want" && "$got" != "$rev" ]]; then
  echo "ci-seed-schema: HEAD ${got} != wrap ${rev}" >&2
  exit 1
fi
test -f "${dest}/schema/policy.capnp"
test -f "${dest}/meson.build"
echo "GROKOS_SCHEMA_SOT=${dest} sha=${got} wrap=${rev}"
