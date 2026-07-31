# Host hygiene verbs — delegate to grokos-tools (101).
# Monorepo: sibling under grokos-packages/. CI uses tools include jobs instead.

set shell := ["bash", "-euo", "pipefail", "-c"]

tools := justfile_directory() + "/../grokos-tools"
# policyd path is grok-policyd
tools_alt := justfile_directory() + "/../grokos-tools"

default:
    @just --list

check-signed-commits:
    #!/usr/bin/env bash
    set -euo pipefail
    t="{{tools}}"
    if [[ ! -f "${t}/justfile" ]]; then t="$(cd "{{justfile_directory()}}/.." && pwd)/grokos-tools"; fi
    if [[ -f "${t}/justfile" ]]; then
      just --justfile "${t}/justfile" --working-directory "{{justfile_directory()}}" check-signed-commits
    else
      echo "clone/bootstrap grokos-tools sibling, or rely on CI package-validate include" >&2
      exit 1
    fi

check-secrets:
    #!/usr/bin/env bash
    set -euo pipefail
    t="{{tools}}"
    if [[ ! -f "${t}/justfile" ]]; then t="$(cd "{{justfile_directory()}}/.." && pwd)/grokos-tools"; fi
    just --justfile "${t}/justfile" --working-directory "{{justfile_directory()}}" check-secrets-ci
