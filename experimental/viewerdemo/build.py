#!/usr/bin/env python3
"""Build and validate the AV2 quality viewer site."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEMO = Path(__file__).resolve().parent
DEFAULT_SITE = DEMO / "site"


def default_encoder() -> Path:
    directory = ROOT / "build" / "reference" / "avm"
    names = (
        ("avmenc.exe", "avmenc")
        if os.name == "nt"
        else ("avmenc", "avmenc.exe")
    )
    for name in names:
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return directory / names[0]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--encoder", type=Path, default=default_encoder())
    parser.add_argument("--jobs", type=int, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--site-directory", type=Path, default=DEFAULT_SITE)
    parser.add_argument(
        "--skip-assets",
        action="store_true",
        help="Reuse the manifest and OBU files already in the site directory.",
    )
    parser.add_argument(
        "--resume-assets",
        action="store_true",
        help="Reuse completed OBU files while regenerating the asset series.",
    )
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    return args


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> None:
    args = parse_args()
    site = args.site_directory.resolve()
    site.mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "styles.css", "app.js"):
        shutil.copy2(DEMO / name, site / name)

    if not args.skip_assets:
        command = [
            sys.executable,
            str(DEMO / "generate_assets.py"),
            "--output",
            str(site),
            "--encoder",
            str(args.encoder),
            "--jobs",
            str(args.jobs),
        ]
        if args.resume_assets:
            command.append("--resume")
        run(command)

    run(
        [
            sys.executable,
            str(ROOT / "experimental" / "wasm" / "build.py"),
            "--clang",
            args.clang,
            "--output-directory",
            str(site),
        ]
    )
    run([sys.executable, str(DEMO / "validate_site.py"), str(site)])
    for path in sorted(site.iterdir()):
        if path.is_file():
            print(f"{path.name}\t{path.stat().st_size}")


if __name__ == "__main__":
    main()