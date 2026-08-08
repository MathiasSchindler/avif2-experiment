#!/usr/bin/env python3
"""Build the freestanding AV2 decoder WebAssembly module."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = ROOT / "docs"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--clang",
        default="clang",
        help="Clang executable or path (default: clang from PATH).",
    )
    parser.add_argument(
        "--output-directory",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Directory that receives decoder.wasm (default: docs).",
    )
    return parser.parse_args()


def require_tool(value: str) -> str:
    candidate = Path(value).expanduser()
    if candidate.is_file():
        return str(candidate.resolve())
    resolved = shutil.which(value)
    if resolved is None:
        raise RuntimeError(f"missing required tool: {value}")
    return resolved


def core_sources() -> list[Path]:
    cmake_file = ROOT / "CMakeLists.txt"
    cmake = cmake_file.read_text(encoding="utf-8")
    match = re.search(
        r"set\s*\(\s*AV2HEIF_CORE_SOURCES\b(?P<body>.*?)\)",
        cmake,
        flags=re.DOTALL,
    )
    if match is None:
        raise RuntimeError("could not read AV2HEIF_CORE_SOURCES from CMakeLists.txt")
    sources = [
        ROOT / token
        for token in re.findall(r"[^\s()]+\.c", match.group("body"))
    ]
    if not sources or any(not source.is_file() for source in sources):
        raise RuntimeError("AV2HEIF_CORE_SOURCES contains a missing source file")
    return sources


def main() -> None:
    args = parse_args()
    clang = require_tool(args.clang)
    output_directory = args.output_directory.resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    output = output_directory / "decoder.wasm"
    sources = core_sources() + [Path(__file__).resolve().parent / "decoder_wasm.c"]
    command = [
        clang,
        "--target=wasm32",
        "-std=c11",
        "-O3",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-Wno-tautological-constant-out-of-range-compare",
        "-ffreestanding",
        "-fno-builtin",
        "-fno-stack-protector",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        "-nostdinc",
        f"-I{ROOT / 'include'}",
        f"-I{ROOT / 'src'}",
        f"-I{ROOT / 'src' / 'shared'}",
        "-nostdlib",
        "-Wl,--no-entry",
        "-Wl,--export-memory",
        "-Wl,--initial-memory=16777216",
        "-Wl,--max-memory=536870912",
        "-Wl,--stack-first",
        "-Wl,-z,stack-size=8388608",
        "-Wl,--gc-sections",
        "-o",
        str(output),
        *(str(source) for source in sources),
    ]
    subprocess.run(command, cwd=ROOT, check=True)
    print(f"Built {output} ({output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()