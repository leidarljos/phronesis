#!/usr/bin/env python3
"""Verify and optionally upload build/libgrok_policyd.a to the package registry.

Host entry: ``just publish-lib`` (builds the archive first).

Upload requires CI_API_V4_URL, CI_PROJECT_ID, CI_COMMIT_SHA, CI_JOB_TOKEN.
Set POLICYD_PUBLISH_DRY_RUN=1 to verify symbols only (merge-request continuous integration).
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = Path(os.environ.get("POLICYD_STATIC_LIB", ROOT / "build" / "libgrok_policyd.a"))
PACKAGE = "libgrok_policyd"
FILE = "libgrok_policyd.a"
# Flat Cap'n product surface (API_VERSION 2) + supervisor lifecycle.
NEED_SYMS = (
    "grok_supervisor_open",
    "grok_policyd_status",
    "grok_policyd_check_shell",
)


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(1)


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

    url = (
        f"{os.environ['CI_API_V4_URL']}/projects/{os.environ['CI_PROJECT_ID']}"
        f"/packages/generic/{PACKAGE}/{os.environ['CI_COMMIT_SHA']}/{FILE}"
    )
    data = LIB.read_bytes()
    print(f"publish {len(data)} bytes -> {url}")

    req = urllib.request.Request(
        url,
        data=data,
        method="PUT",
        headers={
            "JOB-TOKEN": os.environ["CI_JOB_TOKEN"],
            "Content-Type": "application/octet-stream",
        },
    )
    try:
        with urllib.request.urlopen(req) as resp:
            body = resp.read().decode()
            if body:
                print(body)
    except urllib.error.HTTPError as e:
        err = e.read().decode()
        if err:
            print(err, file=sys.stderr)
        die(f"package upload failed (http={e.code})")


def main() -> None:
    verify_archive()
    if os.environ.get("POLICYD_PUBLISH_DRY_RUN") == "1":
        print("dry-run: skip package registry upload")
        return
    upload()


if __name__ == "__main__":
    main()
