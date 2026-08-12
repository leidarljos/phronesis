set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

# Meson only (no pixi, no private channels). Needs capnp + cmocka on PATH.
meson-test:
    #!/usr/bin/env bash
    if [[ -f build/build.ninja ]]; then
      meson setup build --reconfigure
    else
      meson setup build
    fi
    meson compile -C build
    meson test -C build --print-errorlogs

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

# Optional: refresh schema/ from a sibling schema checkout.
sync-schema src:
    bash scripts/sync-schema-from-sibling.sh "{{src}}"
