set shell := ["bash", "-euo", "pipefail", "-c"]

mod? tools "../grokos-tools"

default:
    @just --list

# Package native tasks (pixi).
test:
    pixi run test

build:
    pixi run build

lib:
    pixi run lib

# Full static archive → GitLab generic package (CI_JOB_TOKEN + API env).
publish-lib: lib
    pixi run python scripts/publish_static_lib.py

coverage:
    pixi run coverage

ci:
    pixi run ci

# Canonical local CI — tools ci-kit SoT (meta #140). No second runner.
local-ci:
    #!/usr/bin/env -S bash -euo pipefail
    tools="${GROKOS_TOOLS_ROOT:-{{justfile_directory()}}/../grokos-tools}"
    script="${tools}/ci-kit/local-ci/local-ci.sh"
    [[ -f "$script" ]] || {
      echo "local-ci: missing ${script} (sibling grokos-tools or GROKOS_TOOLS_ROOT)" >&2
      exit 1
    }
    cd "{{justfile_directory()}}"
    bash "$script"

# Optional: refresh schema/ from a sibling grokos-schema checkout.
sync-schema src="":
    #!/usr/bin/env bash
    if [[ -n "{{src}}" ]]; then
      bash scripts/sync-schema-from-sibling.sh "{{src}}"
    else
      bash scripts/sync-schema-from-sibling.sh
    fi
