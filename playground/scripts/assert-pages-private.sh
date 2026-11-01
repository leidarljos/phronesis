#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Checklist / soft assert: GitLab Pages for the playground must be members-only.
# Project visibility of the site is a GitLab *project setting*, not CI YAML.
# This script documents the required setting and optionally queries the API.
#
# Usage:
#   bash playground/scripts/assert-pages-private.sh           # print checklist
#   bash playground/scripts/assert-pages-private.sh --check   # API assert if token
#
# Exit codes:
#   0  checklist printed, or API reports private/members-only
#   1  --check and pages_access_level is not private
#   2  --check requested but no API access (token / glab / project id)
#
# Expected project setting (UI):
#   Settings → General → Visibility → Pages
#     Access level = "Only project members"
#   Do NOT attach a public custom domain for the playground.
#   Do NOT set Pages to "Everyone" / public.
#
set -euo pipefail

MODE="${1:-}"
if [[ "${MODE}" != "" && "${MODE}" != "--check" && "${MODE}" != "-h" && "${MODE}" != "--help" ]]; then
	echo "usage: $0 [--check]" >&2
	exit 2
fi

if [[ "${MODE}" == "-h" || "${MODE}" == "--help" ]]; then
	sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
	exit 0
fi

cat <<'EOF'
=== Playground GitLab Pages — members-only checklist ===

1. Project UI
   Settings → General → Visibility, project features, permissions
     → Pages → Access level: "Only project members"
   (API field: pages_access_level == "private")

2. Do not publish the playground behind a public custom domain.
   Package path (base) is enough: /phronesis/ under the group Pages host.
   Custom domains that make the site world-readable defeat members-only.

3. CI job `pages` publishes public/ from playground/astro/dist after
   playground/scripts/build-site.sh. It needs playground/dist-wasm artifacts
   (from playground:wasm on emsdk, or rg.terra: pixi run playground-wasm).

4. Confirm after deploy (as a non-member / private window):
   - Unauthenticated GET of the Pages URL returns 401/403/login, not 200 HTML.
   - Authenticated project member can open /play and load the evaluator.

5. Optional API check (this script --check):
   - GITLAB_TOKEN or CI_JOB_TOKEN + CI_API_V4_URL + CI_PROJECT_ID, or glab auth
   - Expect pages_access_level: private

EOF

if [[ "${MODE}" != "--check" ]]; then
	echo "(pass --check to query the API when credentials are available)"
	exit 0
fi

# --- optional API assert ---------------------------------------------------

api_get_project() {
	local url base token project
	if command -v glab >/dev/null 2>&1; then
		# glab api uses the authenticated host; path is relative to /api/v4
		if [[ -n "${CI_PROJECT_ID:-}" ]]; then
			glab api "projects/${CI_PROJECT_ID}" 2>/dev/null && return 0
		fi
		if [[ -n "${CI_PROJECT_PATH:-}" ]]; then
			# URL-encode path: group%2Fproject
			local enc
			enc="$(printf '%s' "${CI_PROJECT_PATH}" | sed 's|/|%2F|g')"
			glab api "projects/${enc}" 2>/dev/null && return 0
		fi
		# Fallback: current git remote project if glab knows it
		glab api "projects/:fullpath" 2>/dev/null && return 0
	fi

	base="${CI_API_V4_URL:-${GITLAB_API_URL:-}}"
	project="${CI_PROJECT_ID:-${GITLAB_PROJECT_ID:-}}"
	token="${GITLAB_TOKEN:-${PRIVATE_TOKEN:-${CI_JOB_TOKEN:-}}}"
	if [[ -z "${base}" || -z "${project}" || -z "${token}" ]]; then
		return 1
	fi
	# CI_JOB_TOKEN uses JOB-TOKEN header; personal tokens use PRIVATE-TOKEN
	if [[ -n "${CI_JOB_TOKEN:-}" && "${token}" == "${CI_JOB_TOKEN}" ]]; then
		curl -fsS --header "JOB-TOKEN: ${token}" \
			"${base}/projects/${project}"
	else
		curl -fsS --header "PRIVATE-TOKEN: ${token}" \
			"${base}/projects/${project}"
	fi
}

if ! command -v jq >/dev/null 2>&1; then
	echo "FATAL: jq required for --check" >&2
	exit 2
fi

json="$(api_get_project)" || {
	echo "FATAL: cannot read project via glab/API." >&2
	echo "  Set GITLAB_TOKEN + CI_API_V4_URL + CI_PROJECT_ID, or run glab auth login." >&2
	exit 2
}

level="$(printf '%s' "${json}" | jq -r '.pages_access_level // empty')"
path_with_namespace="$(printf '%s' "${json}" | jq -r '.path_with_namespace // empty')"

echo "project: ${path_with_namespace:-unknown}"
echo "pages_access_level: ${level:-<missing>}"

case "${level}" in
private)
	echo "OK: Pages access is members-only (private)."
	exit 0
	;;
disabled)
	echo "NOTE: Pages is disabled (not public; also not deployed)."
	exit 0
	;;
enabled | public)
	echo "FAIL: Pages access is '${level}' — must be 'private' (Only project members)." >&2
	echo "  Fix: Settings → General → Pages → Only project members" >&2
	echo "  or:  glab api --method PUT projects/:id -f pages_access_level=private" >&2
	exit 1
	;;
*)
	echo "FAIL: unexpected or missing pages_access_level='${level}'" >&2
	exit 1
	;;
esac
