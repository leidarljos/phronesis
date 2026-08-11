#!/usr/bin/env bash
# Pre-seed subprojects/grokos-schema for Meson subproject (CI job-token).
# Never put CI_JOB_TOKEN in the URL: a colon in the token is parsed as a port.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
# Needle is split so this file does not contain the forbidden construction.
if grep -RInE --include='*.sh' --include='*.py' \
  'gitlab-ci-token:[$][{]CI_JOB_TOKEN[}]' "$here"; then
  echo "ci-seed-schema: token-in-URL construction returned (meta #135)" >&2
  exit 2
fi

git_ci() {
  local restore_x=0 rc=0 ask="" host
  case $- in *x*) restore_x=1; set +x ;; esac
  if [[ -n "${CI_JOB_TOKEN:-}" ]]; then
    ask="$(mktemp)"
    printf '%s\n' '#!/bin/sh' 'printf %s "$CI_JOB_TOKEN"' >"$ask"
    chmod 700 "$ask"
    host="${CI_SERVER_HOST:-nova.teachx.ai}"
    GIT_ASKPASS="$ask" GIT_TERMINAL_PROMPT=0 \
      git -c credential.helper= \
      -c "url.https://gitlab-ci-token@${host}/.insteadOf=https://${host}/" \
      "$@" || rc=$?
    rm -f "$ask"
  else
    git "$@" || rc=$?
  fi
  [[ "$restore_x" -eq 1 ]] && set -x
  return "$rc"
}

dest="${CI_PROJECT_DIR}/subprojects/grokos-schema"
if [[ -f "${dest}/schema/policy.capnp" && -f "${dest}/meson.build" ]]; then
  echo "schema SoT already present at ${dest}"
  git -C "${dest}" rev-parse --short HEAD || true
  exit 0
fi
: "${CI_JOB_TOKEN:?CI_JOB_TOKEN required}"
: "${CI_SERVER_HOST:?CI_SERVER_HOST required}"
url="https://${CI_SERVER_HOST}/trace-analysis/grokos-packages/grokos-schema.git"
rm -rf "${dest}"
mkdir -p "${CI_PROJECT_DIR}/subprojects"
rev=""
if [[ -f subprojects/grokos-schema.wrap ]]; then
  rev=$(grep -E '^revision' subprojects/grokos-schema.wrap | awk '{print $3}' | tr -d '[:space:]' || true)
fi
echo "ci-seed-schema: clone schema SoT (no token in URL; no blob filter)"
git_ci clone "$url" "${dest}"
if [[ -z "${rev}" ]]; then
  echo "ci-seed-schema: wrap missing revision" >&2
  exit 1
fi
git_ci -C "${dest}" fetch origin "${rev}" \
  || git_ci -C "${dest}" fetch --depth 1 origin "${rev}" \
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
