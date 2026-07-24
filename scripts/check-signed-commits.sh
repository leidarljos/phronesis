#!/usr/bin/env bash
# Fail if any non-merge commit in the MR (or push range) lacks a *verified*
# GitLab signature (GPG or SSH). Uses the GitLab signature API so we don't
# need an allowed_signers file in CI.
#
# Env (CI provides these):
#   CI_API_V4_URL, CI_PROJECT_ID, CI_JOB_TOKEN (or GITLAB_TOKEN / PRIVATE_TOKEN)
#   CI_MERGE_REQUEST_DIFF_BASE_SHA / CI_COMMIT_BEFORE_SHA / CI_COMMIT_SHA
set -euo pipefail

API="${CI_API_V4_URL:-https://nova.teachx.ai/api/v4}"
PROJECT="${CI_PROJECT_ID:?CI_PROJECT_ID required}"
TOKEN="${CI_JOB_TOKEN:-${GITLAB_TOKEN:-${PRIVATE_TOKEN:-}}}"
if [[ -z "${TOKEN}" ]]; then
  echo "ERROR: need CI_JOB_TOKEN or GITLAB_TOKEN to check signatures" >&2
  exit 2
fi

# Prefer JOB-TOKEN header when using CI_JOB_TOKEN
if [[ -n "${CI_JOB_TOKEN:-}" && "${TOKEN}" == "${CI_JOB_TOKEN}" ]]; then
  AUTH_HEADER=(--header "JOB-TOKEN: ${TOKEN}")
else
  AUTH_HEADER=(--header "PRIVATE-TOKEN: ${TOKEN}")
fi

BASE="${CI_MERGE_REQUEST_DIFF_BASE_SHA:-}"
if [[ -z "${BASE}" || "${BASE}" == "0000000000000000000000000000000000000000" ]]; then
  BASE="${CI_COMMIT_BEFORE_SHA:-}"
fi
HEAD_SHA="${CI_COMMIT_SHA:-$(git rev-parse HEAD)}"

COMMITS_FILE=$(mktemp)
trap 'rm -f "${COMMITS_FILE}"' EXIT

if [[ -z "${BASE}" || "${BASE}" == "0000000000000000000000000000000000000000" ]]; then
  # First push / unknown base: only check HEAD (skip if it is a merge)
  parents=$(git rev-list --parents -n 1 "${HEAD_SHA}" | wc -w | tr -d ' ')
  if [[ "${parents}" -gt 2 ]]; then
    echo "HEAD is a merge commit and base is unknown; skipping."
    exit 0
  fi
  echo "${HEAD_SHA}" > "${COMMITS_FILE}"
else
  git rev-list --no-merges "${BASE}..${HEAD_SHA}" > "${COMMITS_FILE}" 2>/dev/null || true
  if [[ ! -s "${COMMITS_FILE}" ]]; then
    # Merge-only push (e.g. GitLab merge commit) — nothing author-signed to check
    echo "No non-merge commits in range ${BASE:0:8}..${HEAD_SHA:0:8}; OK."
    exit 0
  fi
fi

if [[ ! -s "${COMMITS_FILE}" ]]; then
  echo "No non-merge commits to check."
  exit 0
fi

count=$(wc -l < "${COMMITS_FILE}" | tr -d ' ')
echo "Checking ${count} non-merge commit(s) for verified signatures..."
failed=0
while IFS= read -r sha; do
  [[ -z "${sha}" ]] && continue
  # signature endpoint: 200 + verification_status=verified → ok
  # 404 → no signature
  code=$(curl -sS -o /tmp/sig.json -w "%{http_code}" \
    "${AUTH_HEADER[@]}" \
    "${API}/projects/${PROJECT}/repository/commits/${sha}/signature" || echo "000")
  status=""
  if [[ "${code}" == "200" ]]; then
    status=$(python3 -c "import json; print(json.load(open('/tmp/sig.json')).get('verification_status',''))" 2>/dev/null || true)
  fi
  short="${sha:0:8}"
  if [[ "${code}" == "200" && "${status}" == "verified" ]]; then
    echo "  OK  ${short}  verified"
  else
    echo "  FAIL ${short}  unsigned or unverified (HTTP ${code}, status=${status:-n/a})"
    failed=1
  fi
done < "${COMMITS_FILE}"

if [[ "${failed}" -ne 0 ]]; then
  cat <<'MSG' >&2

ERROR: unsigned (or unverified) commits are not allowed on GrokOS repos.

Setup (SSH signing recommended — works with a key already on GitLab):

  git config --global gpg.format ssh
  git config --global user.signingkey ~/.ssh/id_ed25519.pub   # or your key
  git config --global commit.gpgsign true
  # Upload the *same* public key on GitLab → Preferences → SSH Keys
  # with usage "Authentication & Signing" (or Signing).

Or GPG:

  git config --global user.signingkey <KEYID>
  git config --global commit.gpgsign true
  # Upload public key: GitLab → Preferences → GPG Keys

Agents: do not disable commit.gpgsign. Prefer not committing unless asked;
when you commit, the ambient git config must sign.

See CONTRIBUTING.md § Signed commits.
MSG
  exit 1
fi

echo "All checked commits have verified signatures."
