#!/usr/bin/env python3
"""Upload build/libgrok_policyd.a to this project's generic package registry.

Host entry: ``just publish-lib`` (builds the archive first).
Requires CI_API_V4_URL, CI_PROJECT_ID, CI_COMMIT_SHA, CI_JOB_TOKEN.
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
NEED_SYM = "grok_policyd_handle_capnp"
PACKAGE = "libgrok_policyd"
FILE = "libgrok_policyd.a"
# nm symbol lines: " T name" / "00000000 T name" etc. — whole token only.
_SYM_LINE = re.compile(rf"\b{re.escape(NEED_SYM)}\b")


def die(msg: str) -> None:
    print(f"error: {msg}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if not LIB.is_file() or LIB.stat().st_size == 0:
        die(f"missing or empty archive: {LIB}")

    nm = subprocess.run(
        ["nm", "-g", str(LIB)],
        check=False,
        capture_output=True,
        text=True,
    )
    if nm.returncode != 0:
        die(f"nm failed on {LIB}: {nm.stderr or nm.stdout}")
    if not _SYM_LINE.search(nm.stdout):
        die(f"{LIB} missing global symbol {NEED_SYM}")

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
            body = resp.read().decode()
            if body:
                print(body)
    except urllib.error.HTTPError as e:
        err = e.read().decode()
        if err:
            print(err, file=sys.stderr)
        die(f"package upload failed (http={e.code})")


if __name__ == "__main__":
    main()
