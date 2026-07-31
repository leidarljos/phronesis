set shell := ["bash", "-euo", "pipefail", "-c"]

mod? tools "../grokos-tools"

default:
    @just --list

test:
    pixi run test

build:
    pixi run build

ci:
    pixi run ci


# Host continuous integration: tools package-validate (tools main).
