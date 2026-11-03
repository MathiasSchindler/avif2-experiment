#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SAMPLE_INCLUDES = {
    "sample_kernel_u8.inc", "sample_kernel_u16.inc", "sample_kernel_end.inc",
}


def source_paths(root: Path = ROOT) -> list[Path]:
    return sorted(
        path for directory in ("src", "include")
        for path in (root / directory).rglob("*")
        if path.suffix in {".c", ".h"} or path.name.endswith("_kernel.inc")
        or path.name in SAMPLE_INCLUDES
    )


def run_formatter(
    paths: list[Path], *, write: bool = False, formatter: str | None = None,
) -> subprocess.CompletedProcess[str]:
    executable = formatter or os.environ.get("CLANG_FORMAT", "clang-format")
    version = subprocess.run(
        [executable, "--version"], text=True, capture_output=True, check=True,
    ).stdout.strip()
    if not re.search(r"\bclang-format version 22\.", version):
        raise RuntimeError(f"clang-format 22.x required; {executable} reports: {version}")
    if not paths:
        raise RuntimeError("no production C sources selected for formatting")
    return subprocess.run(
        [executable, f"--style=file:{ROOT / '.clang-format'}",
         *(["-i"] if write else ["--dry-run", "--Werror"]),
         *(str(path) for path in paths)],
        cwd=ROOT, text=True, capture_output=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Check canonical production C formatting.")
    parser.add_argument("--write", action="store_true", help="format sources in place")
    parser.add_argument("--clang-format", help="clang-format 22.x executable (or CLANG_FORMAT)")
    args = parser.parse_args()
    try:
        paths = source_paths()
        result = run_formatter(paths, write=args.write, formatter=args.clang_format)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"formatting: {error}", file=sys.stderr)
        return 1
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode == 0:
        print(f"formatting: {len(paths)} production files {'formatted' if args.write else 'checked'}")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
