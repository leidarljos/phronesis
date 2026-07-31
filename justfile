# Host hygiene via grokos-tools module (issue 101).
# CI: include tools package-validate. Local: monorepo sibling.

set shell := ["bash", "-euo", "pipefail", "-c"]

mod? tools "../grokos-tools"

default:
    @just --list

check-signed-commits:
    just tools check-signed-commits

check-secrets:
    just tools check-secrets-ci

