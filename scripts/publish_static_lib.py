#!/usr/bin/env python3
"""Verify and optionally upload build/libgrok_policyd.a to the package registry.

Host entry: ``just publish-lib`` (builds the archive first).

Upload uses curl (same pattern as tools generic package publish), with JOB-TOKEN.
Set POLICYD_PUBLISH_DRY_RUN=1 to verify symbols only (merge-request continuous integration).
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = Path(os.environ.get("POLICYD_STATIC_LIB", ROOT / "build" / "libgrok_policyd.a"))
PACKAGE = "libgrok_policyd"
FILE = "libgrok_policyd.a"
# Flat Cap'n product surface (API_VERSION 3) + supervisor lifecycle.
NEED_SYMS = (
    "policyd_supervisor_open",
    "policyd_status",
    "policyd_check_shell",
)


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(1)


def schema_pin_path() -> Path:
    """Ship the in-tree interface stamp next to the archive."""
    local = ROOT / "schema" / "SCHEMA_PIN"
    if not local.is_file():
        die(f"missing {local}")
    return local


def verify_archive() -> None:
    if not LIB.is_file() or LIB.stat().st_size == 0:
        die(f"missing or empty archive: {LIB}")

    nm = subprocess.run(
        ["nm", str(LIB)],
        check=False,
        capture_output=True,
        text=True,
    )
    if nm.returncode != 0:
        die(f"nm failed on {LIB}: {nm.stderr or nm.stdout}")
    for sym in NEED_SYMS:
        if not re.search(rf"\b{re.escape(sym)}\b", nm.stdout):
            die(f"{LIB} missing symbol {sym}")
    print(f"ok: {LIB} ({LIB.stat().st_size} bytes) exports {', '.join(NEED_SYMS)}")


def upload() -> None:
    for key in ("CI_API_V4_URL", "CI_PROJECT_ID", "CI_COMMIT_SHA", "CI_JOB_TOKEN"):
        if not os.environ.get(key):
            die(f"{key} is required for upload")

    curl = shutil.which("curl")
    if not curl:
        die("curl not found on PATH")

    sha = os.environ["CI_COMMIT_SHA"]
    base = (
        f"{os.environ['CI_API_V4_URL']}/projects/{os.environ['CI_PROJECT_ID']}"
        f"/packages/generic/{PACKAGE}/{sha}"
    )
    pin = schema_pin_path()
    for path in (LIB, pin):
        remote = path.name
        url = f"{base}/{remote}"
        print(f"publish {path.stat().st_size} bytes -> {url}")
        out = Path("/tmp/publish-static-lib.out")
        proc = subprocess.run(
            [
                curl,
                "-sS",
                "-o",
                str(out),
                "-w",
                "%{http_code}",
                "--header",
                f"JOB-TOKEN: {os.environ['CI_JOB_TOKEN']}",
                "--upload-file",
                str(path),
                url,
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        code = (proc.stdout or "").strip()
        body = out.read_text() if out.is_file() else ""
        if body:
            print(body)
        if proc.returncode != 0:
            die(f"curl failed (exit {proc.returncode}): {proc.stderr or proc.stdout}")
        if code not in ("200", "201"):
            die(f"package upload failed (http={code})")
        print(f"http={code}")


def main() -> None:
    verify_archive()
    pin = schema_pin_path()
    print(f"schema pin: {pin.read_text().strip()}")
    if os.environ.get("POLICYD_PUBLISH_DRY_RUN") == "1":
        print("dry-run: skip package registry upload")
        return
    upload()


if __name__ == "__main__":
    main()
