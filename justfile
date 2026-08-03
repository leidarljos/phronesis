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
