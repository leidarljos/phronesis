# SPDX-License-Identifier: Apache-2.0
"""Sphinx configuration for grok-policyd (Doxygen XML via breathe)."""

from __future__ import annotations

import os
import subprocess
from datetime import datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DOCS = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def run_doxygen() -> None:
    subprocess.run(["doxygen", "Doxyfile"], cwd=DOCS, check=True)


run_doxygen()

project = "grok-policyd"
author = "GrokOS contributors"
copyright = f"{datetime.now().year}, {author}"
release = "0.1.0"
version = "0.1"

extensions = [
    "breathe",
    "myst_parser",
    "sphinx.ext.intersphinx",
]

templates_path: list[str] = []
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

html_theme = "furo"
html_title = "grok-policyd"
html_static_path: list[str] = []

breathe_projects = {
    "grok-policyd": os.path.join(DOCS, "build", "doxygen", "xml"),
}
breathe_default_project = "grok-policyd"
breathe_domain_by_extension = {
    "h": "c",
    "c": "c",
}

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}
