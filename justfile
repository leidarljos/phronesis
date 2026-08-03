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

coverage:
    pixi run coverage

ci:
    pixi run ci
