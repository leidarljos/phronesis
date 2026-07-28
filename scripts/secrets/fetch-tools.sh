#!/usr/bin/env bash
# Bootstrap: resolve grokos-tools kit directory.
#
# Outputs absolute path to ci-kit/secrets on stdout.
#
# Env:
#   SECRETS_TOOLS_PIN     full tools commit SHA (required in CI; default for pin fetch)
#   SECRETS_TOOLS_PATH    local override → tools repo root (REFUSED when $CI is set)
#   SECRETS_TOOLS_CACHE   clone cache dir (default: ${TMPDIR:-/tmp}/grokos-tools-secrets-pin)
#   SECRETS_TOOLS_URL     clone URL (default: same-host GitLab path)
#
# Exit 2 on misuse / infra failure.
set -euo pipefail

log() { printf '%s\n' "$*" >&2; }
die() { log "ERROR: $*"; exit 2; }

# CI must never use a host path override (reproducibility / pin integrity).
if [[ -n "${CI:-}" && -n "${SECRETS_TOOLS_PATH:-}" ]]; then
  die "SECRETS_TOOLS_PATH is refused when CI is set; use SECRETS_TOOLS_PIN only"
fi

KIT_SUBPATH="ci-kit/secrets"

if [[ -n "${SECRETS_TOOLS_PATH:-}" ]]; then
  tools="$(cd "${SECRETS_TOOLS_PATH}" && pwd)"
  [[ -d "${tools}/${KIT_SUBPATH}" ]] || die "SECRETS_TOOLS_PATH missing ${KIT_SUBPATH}: ${tools}"
  [[ -x "${tools}/${KIT_SUBPATH}/check-no-secrets.sh" ]] || die "kit entrypoint not executable under ${tools}/${KIT_SUBPATH}"
  printf '%s\n' "${tools}/${KIT_SUBPATH}"
  exit 0
fi

# Read pin file: first line that is exactly a 40-char lowercase SHA.
# Skips blanks and #-comments. Does not squash the file with tr (comments
# must not be concatenated into the pin).
read_pin_file() {
  local f="$1" line
  [[ -f "$f" ]] || return 1
  while IFS= read -r line || [[ -n "$line" ]]; do
    # trim CR then surrounding whitespace
    line="${line//$'\r'/}"
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [[ -z "$line" || "$line" == \#* ]] && continue
    if [[ "$line" =~ ^[0-9a-f]{40}$ ]]; then
      printf '%s\n' "$line"
      return 0
    fi
    die "SECRETS_TOOLS_PIN file ${f}: expected a full 40-char lowercase git SHA line (got: ${line})"
  done <"$f"
  return 1
}

pin="${SECRETS_TOOLS_PIN:-}"
if [[ -n "$pin" ]]; then
  [[ "$pin" =~ ^[0-9a-f]{40}$ ]] || die "SECRETS_TOOLS_PIN must be a full 40-char lowercase git SHA (got: ${pin})"
else
  pin=""
  if [[ -f SECRETS_TOOLS_PIN ]]; then
    pin="$(read_pin_file SECRETS_TOOLS_PIN)" || die "SECRETS_TOOLS_PIN file has no valid 40-char pin line (blanks/#-comments only?)"
  elif [[ -f "$(dirname "$0")/../SECRETS_TOOLS_PIN" ]]; then
    pin="$(read_pin_file "$(dirname "$0")/../SECRETS_TOOLS_PIN")" || die "SECRETS_TOOLS_PIN file has no valid 40-char pin line (blanks/#-comments only?)"
  elif [[ -f "$(dirname "$0")/../../SECRETS_TOOLS_PIN" ]]; then
    # package root when script is vendored as scripts/secrets/fetch-tools.sh
    pin="$(read_pin_file "$(dirname "$0")/../../SECRETS_TOOLS_PIN")" || die "SECRETS_TOOLS_PIN file has no valid 40-char pin line (blanks/#-comments only?)"
  fi
  [[ -n "$pin" ]] || die "SECRETS_TOOLS_PIN unset (and no SECRETS_TOOLS_PATH for local)"
fi

url="${SECRETS_TOOLS_URL:-git@ssh.nova.teachx.ai:trace-analysis/grokos-packages/grokos-tools.git}"
# Prefer HTTPS job-token form in CI when CI_JOB_TOKEN present
if [[ -n "${CI_JOB_TOKEN:-}" && -n "${CI_SERVER_HOST:-}" ]]; then
  url="https://gitlab-ci-token:${CI_JOB_TOKEN}@${CI_SERVER_HOST}/trace-analysis/grokos-packages/grokos-tools.git"
fi

cache="${SECRETS_TOOLS_CACHE:-${TMPDIR:-/tmp}/grokos-tools-secrets-pin}"
dest="${cache}/${pin}"
mkdir -p "$cache"

if [[ ! -d "${dest}/.git" ]]; then
  log "fetch-tools: cloning tools@${pin}"
  rm -rf "$dest"
  git clone --no-checkout --filter=blob:none "$url" "$dest" >&2
  git -C "$dest" fetch --depth 1 origin "$pin" >&2 || git -C "$dest" fetch origin "$pin" >&2
  git -C "$dest" checkout --force "$pin" >&2
else
  if ! git -C "$dest" cat-file -e "${pin}^{commit}" 2>/dev/null; then
    git -C "$dest" fetch origin "$pin" >&2 || git -C "$dest" fetch --depth 1 origin "$pin" >&2
  fi
  git -C "$dest" checkout --force "$pin" >&2
fi

[[ -x "${dest}/${KIT_SUBPATH}/check-no-secrets.sh" ]] || die "tools@${pin} missing kit at ${KIT_SUBPATH}"
printf '%s\n' "${dest}/${KIT_SUBPATH}"
