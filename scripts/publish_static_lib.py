#!/usr/bin/env python3
"""Upload build/libgrok_policyd.a to this project's generic package registry.

Host entry: ``just publish-lib`` (builds the archive first).
Requires CI_API_V4_URL, CI_PROJECT_ID, CI_COMMIT_SHA, CI_JOB_TOKEN.
"""
from __future__ import annotations

import os
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = Path(os.environ.get("POLICYD_STATIC_LIB", ROOT / "build" / "libgrok_policyd.a"))
NEED_SYM = "grok_policyd_handle_capnp"
PACKAGE = "libgrok_policyd"
FILE = "libgrok_policyd.a"


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if not LIB.is_file() or LIB.stat().st_size == 0:
        die(f"missing or empty archive: {LIB}")

    nm = subprocess.run(["nm", str(LIB)], check=False, capture_output=True, text=True)
    if nm.returncode != 0:
        die(f"nm failed on {LIB}: {nm.stderr or nm.stdout}")
    if NEED_SYM not in nm.stdout:
        die(f"{LIB} missing symbol {NEED_SYM}")

    for key in ("CI_API_V4_URL", "CI_PROJECT_ID", "CI_COMMIT_SHA", "CI_JOB_TOKEN"):
        if not os.environ.get(key):
            die(f"{key} is required")

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
            print(resp.read().decode())
            if resp.status not in (200, 201):
                die(f"package upload failed (http={resp.status})")
    except urllib.error.HTTPError as e:
        err = e.read().decode()
        if err:
            print(err, file=sys.stderr)
        die(f"package upload failed (http={e.code})")


if __name__ == "__main__":
    main()
