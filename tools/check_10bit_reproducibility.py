#!/usr/bin/env python3
"""Regenerate the WP10 corpus and require byte-identical output."""

from __future__ import annotations

import argparse
import hashlib
import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CORPUS = ROOT / "tests" / "fixtures" / "10bit"


def snapshot(directory: Path) -> dict[str, str]:
    return {
        path.relative_to(directory).as_posix(): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(directory.rglob("*"))
        if path.is_file()
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, default=DEFAULT_CORPUS)
    parser.add_argument(
        "--make-program",
        default=os.environ.get("MAKE", "make"),
        help="make executable used to run the 10bit-wp10 target",
    )
    args = parser.parse_args()

    corpus = args.corpus.resolve()
    if not corpus.is_dir():
        raise SystemExit(f"10-bit corpus directory does not exist: {corpus}")
    before = snapshot(corpus)
    if not before:
        raise SystemExit(f"10-bit corpus is empty: {corpus}")

    subprocess.run(
        [args.make_program, "10bit-wp10"],
        cwd=ROOT,
        check=True,
    )
    after = snapshot(corpus)
    if before != after:
        before_paths = set(before)
        after_paths = set(after)
        for path in sorted(before_paths - after_paths):
            print(f"removed: {path}")
        for path in sorted(after_paths - before_paths):
            print(f"added: {path}")
        for path in sorted(before_paths & after_paths):
            if before[path] != after[path]:
                print(f"changed: {path}")
        return 1

    print(f"WP10 corpus reproducible: {len(after)} files byte-identical")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
