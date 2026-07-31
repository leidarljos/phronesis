# Host hygiene — delegate to grokos-tools (same verbs CI runs via package-validate).

set shell := ["bash", "-euo", "pipefail", "-c"]

tools_just := justfile_directory() + "/../grokos-tools/justfile"

default:
    @just --list

check-signed-commits:
    just --justfile {{tools_just}} --working-directory {{justfile_directory()}} check-signed-commits

check-secrets:
    just --justfile {{tools_just}} --working-directory {{justfile_directory()}} check-secrets-ci

