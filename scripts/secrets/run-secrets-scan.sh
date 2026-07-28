#!/usr/bin/env bash
# Optional one-liner for *packages* after vendoring under scripts/secrets/.
# Assumes this file lives at: <repo>/scripts/secrets/run-secrets-scan.sh
#   → repo root is ../.. from this script.
#
# Do not run from the tools kit tree (ci-kit/secrets/…); use
# check-no-secrets.sh / self-test.sh there instead.
set -euo pipefail

log() { printf '%s\n' "$*" >&2; }
die() { log "ERROR: $*"; exit 2; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Refuse if this copy still lives under the tools kit layout (not vendored).
case "${SCRIPT_DIR}" in
  */ci-kit/secrets|*/ci-kit/secrets/*|*/ci-kit/secrets/bootstrap|*/ci-kit/secrets/bootstrap/*)
    die "run-secrets-scan.sh is for packages vendored at scripts/secrets/ — not for use from ci-kit/. From the tools kit run: bash ci-kit/secrets/self-test.sh and/or SECRETS_SCAN_ROOT=\$PWD bash ci-kit/secrets/check-no-secrets.sh"
    ;;
esac
# Also match when script path clearly contains ci-kit/secrets (bootstrap path in kit).
if [[ "${SCRIPT_DIR}" == *"/ci-kit/"* ]]; then
  die "run-secrets-scan.sh is for packages vendored at scripts/secrets/ — not for use from ci-kit/. From the tools kit run: bash ci-kit/secrets/self-test.sh and/or SECRETS_SCAN_ROOT=\$PWD bash ci-kit/secrets/check-no-secrets.sh"
fi

ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
if [[ ! -d "${ROOT}/.git" ]]; then
  die "resolved ROOT has no .git (got: ${ROOT}). run-secrets-scan.sh expects to live at <repo>/scripts/secrets/ so that ../.. is the package git root. Current script dir: ${SCRIPT_DIR}"
fi

cd "$ROOT"
[[ -x scripts/secrets/fetch-tools.sh ]] || die "missing scripts/secrets/fetch-tools.sh under ${ROOT} (vendor fetch-tools.sh next to this script)"
KIT="$(bash scripts/secrets/fetch-tools.sh)"
export SECRETS_SCAN_ROOT="$ROOT"
exec bash "${KIT}/check-no-secrets.sh"
