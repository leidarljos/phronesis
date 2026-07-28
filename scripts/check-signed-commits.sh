#!/usr/bin/env bash
# Fail if any non-merge commit in the MR (or push range) is unsigned.
#
# Primary check: commit object contains a `gpgsig` block (GPG or SSH signing).
# This works in CI without special tokens (CI_JOB_TOKEN cannot read the
# signature API on this GitLab).
#
# Optional strengthen: if PRIVATE_TOKEN/GITLAB_TOKEN is set, also require
# GitLab verification_status=verified.
#
# Env:
#   CI_MERGE_REQUEST_DIFF_BASE_SHA / CI_COMMIT_BEFORE_SHA / CI_COMMIT_SHA
#   CI_API_V4_URL, CI_PROJECT_ID, PRIVATE_TOKEN|GITLAB_TOKEN (optional verify)
set -euo pipefail

BASE="${CI_MERGE_REQUEST_DIFF_BASE_SHA:-}"
if [[ -z "${BASE}" || "${BASE}" == "0000000000000000000000000000000000000000" ]]; then
  BASE="${CI_COMMIT_BEFORE_SHA:-}"
fi
HEAD_SHA="${CI_COMMIT_SHA:-$(git rev-parse HEAD)}"

COMMITS_FILE=$(mktemp)
trap 'rm -f "${COMMITS_FILE}"' EXIT

if [[ -z "${BASE}" || "${BASE}" == "0000000000000000000000000000000000000000" ]]; then
  parents=$(git rev-list --parents -n 1 "${HEAD_SHA}" | wc -w | tr -d ' ')
  if [[ "${parents}" -gt 2 ]]; then
    echo "HEAD is a merge commit and base is unknown; skipping."
    exit 0
  fi
  echo "${HEAD_SHA}" > "${COMMITS_FILE}"
else
  git rev-list --no-merges "${BASE}..${HEAD_SHA}" > "${COMMITS_FILE}" 2>/dev/null || true
  if [[ ! -s "${COMMITS_FILE}" ]]; then
    echo "No non-merge commits in range ${BASE:0:8}..${HEAD_SHA:0:8}; OK."
    exit 0
  fi
fi

if [[ ! -s "${COMMITS_FILE}" ]]; then
  echo "No non-merge commits to check."
  exit 0
fi

API="${CI_API_V4_URL:-}"
PROJECT="${CI_PROJECT_ID:-}"
TOKEN="${GITLAB_TOKEN:-${PRIVATE_TOKEN:-}}"
# Do NOT use CI_JOB_TOKEN here — signature endpoint returns 404 Project Not Found.

count=$(wc -l < "${COMMITS_FILE}" | tr -d ' ')
echo "Checking ${count} non-merge commit(s) for signatures..."
failed=0
while IFS= read -r sha; do
  [[ -z "${sha}" ]] && continue
  short="${sha:0:8}"
  if ! git cat-file -p "${sha}" | grep -q '^gpgsig '; then
    echo "  FAIL ${short}  no signature block in commit object"
    failed=1
    continue
  fi

  if [[ -n "${TOKEN}" && -n "${API}" && -n "${PROJECT}" ]]; then
    code=$(curl -sS -o /tmp/sig.json -w "%{http_code}" \
      --header "PRIVATE-TOKEN: ${TOKEN}" \
      "${API}/projects/${PROJECT}/repository/commits/${sha}/signature" || echo "000")
    status=""
    if [[ "${code}" == "200" ]]; then
      status=$(python3 -c "import json; print(json.load(open('/tmp/sig.json')).get('verification_status',''))" 2>/dev/null || true)
    fi
    if [[ "${code}" == "200" && "${status}" == "verified" ]]; then
      echo "  OK  ${short}  signed + GitLab verified"
    elif [[ "${code}" == "200" ]]; then
      echo "  FAIL ${short}  signed but GitLab status=${status:-unknown}"
      failed=1
    else
      # Token present but API not usable — still accept presence
      echo "  OK  ${short}  signed (GitLab verify skipped: HTTP ${code})"
    fi
  else
    echo "  OK  ${short}  signed"
  fi
done < "${COMMITS_FILE}"

if [[ "${failed}" -ne 0 ]]; then
  cat <<'MSG' >&2

ERROR: unsigned commits are not allowed on GrokOS repos.

Setup (SSH signing recommended):

  ./scripts/setup-commit-signing.sh
  # or:
  git config --global gpg.format ssh
  git config --global user.signingkey ~/.ssh/id_ed25519.pub
  git config --global commit.gpgsign true
  # Upload the same public key on GitLab → Preferences → SSH Keys
  # with usage "Authentication & Signing" (or Signing).

Agents: do not disable commit.gpgsign. Prefer not committing unless asked.

See README.md for signed-commit policy.
MSG
  exit 1
fi

echo "All checked commits are signed."
