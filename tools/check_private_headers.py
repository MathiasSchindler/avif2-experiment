#!/usr/bin/env python3
"""Compile private headers independently and enforce kernel dependencies."""

import argparse
import re
import shlex
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
KERNEL_HEADERS = (
    "block_geometry.h", "coeff_block.h", "leaf_mode.h", "plane.h",
    "prediction.h", "quantizer.h", "reconstruct.h", "transform_type.h",
    "tx_geometry.h", "internal/chroma_prediction.h",
    "internal/decode_diagnostics.h", "internal/intra_prediction.h",
    "internal/intrabc_prediction.h", "internal/reconstruct_block.h",
    "internal/reconstruct_modules.h", "internal/sample_math.h",
    "internal/sample_ops.h", "internal/transform_modules.h",
)
KERNEL_SOURCES = (
    "chroma_prediction.c", "chroma_prediction_cfl.c",
    "chroma_prediction_common.c", "chroma_prediction_mhccp.c",
    "intra_prediction.c", "intra_prediction_basic.c",
    "intra_prediction_dip.c", "intra_prediction_directional.c",
    "intra_prediction_edges.c", "intrabc_prediction.c",
    "inverse_transform.c", "reconstruct.c", "reconstruct_block.c",
    "sample_math.c", "secondary_transform.c", "transform_map.c", "tx_geometry.c",
)


def include_graph(headers: list[Path], directories: list[Path]) -> dict[Path, set[Path]]:
    graph: dict[Path, set[Path]] = {}
    pending = list(headers)
    while pending:
        path = pending.pop()
        if path in graph:
            continue
        graph[path] = set()
        for name in re.findall(
                r'^\s*#\s*include\s+"([^"]+)"',
                path.read_text(encoding="utf-8"), re.MULTILINE):
            dependency = next(
                (candidate.resolve() for directory in [path.parent, *directories]
                 if (candidate := directory / name).is_file()), None)
            if dependency is None:
                raise RuntimeError(f"{path}: missing quoted include {name}")
            graph[path].add(dependency)
            pending.append(dependency)
    return graph


def check_cycles(graph: dict[Path, set[Path]]) -> None:
    visited: set[Path] = set()
    active: list[Path] = []

    def visit(path: Path) -> None:
        if path in active:
            cycle = active[active.index(path):] + [path]
            raise RuntimeError("include cycle: " + " -> ".join(map(str, cycle)))
        if path in visited:
            return
        active.append(path)
        for dependency in sorted(graph[path]):
            visit(dependency)
        active.pop()
        visited.add(path)

    for path in sorted(graph):
        visit(path)


def parse_dependencies(output: str, root: Path) -> set[Path]:
    _, separator, body = output.replace("\\\n", " ").partition(":")
    if not separator:
        raise RuntimeError("compiler returned no dependency rule")
    return {(root / name).resolve() for name in shlex.split(body)}


def check_kernel_dependencies(
    source: Path, dependencies: set[Path], allowed: set[Path],
) -> None:
    forbidden = sorted(
        path for path in dependencies if path.suffix == ".h" and path not in allowed)
    if forbidden:
        raise RuntimeError(
            f"{source}: non-value kernel dependencies: " +
            ", ".join(map(str, forbidden)))


def compile_header(
    compiler: list[str], flags: list[str], source: Path, action: str,
) -> str:
    result = subprocess.run(
        [*compiler, *flags, action,
         *(["-MT", "private-header"] if action == "-MM" else []), "-x", "c", "-"],
        input=f'#include "{source.as_posix()}"\n',
        cwd=ROOT, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(
            f"{source} ({action}):\n{result.stderr.strip() or result.stdout.strip()}")
    return result.stdout


def check(compiler: list[str]) -> None:
    if not compiler:
        raise ValueError("the compiler command must not be empty")
    directories = [ROOT / name for name in ("include", "src", "src/shared")]
    headers = sorted([
        *(ROOT / "src").glob("*.h"), *(ROOT / "src/decoder").rglob("*.h"),
        ROOT / "src/shared/bit_math.h"])
    check_cycles(include_graph(headers, directories))
    kernel_headers = {ROOT / "src/decoder" / name for name in KERNEL_HEADERS}
    allowed = kernel_headers | {
        ROOT / name for name in (
            "include/av2heif.h", "src/base.h", "src/shared/limits.h",
            "src/shared/stddef.h", "src/shared/stdint.h", "src/shared/bit_math.h")}
    flags = [
        "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-ffreestanding", "-nostdinc",
        *(f"-I{path}" for path in directories)]
    for diagnostics in ([], ["-DAV2HEIF_ENABLE_DIAGNOSTICS=1"]):
        for header in headers:
            compile_header(compiler, [*flags, *diagnostics], header, "-fsyntax-only")
        for source in sorted(kernel_headers | {
                ROOT / "src/decoder" / name for name in KERNEL_SOURCES}):
            output = compile_header(compiler, [*flags, *diagnostics], source, "-MM")
            check_kernel_dependencies(source, parse_dependencies(output, ROOT), allowed)
    print(
        f"private headers: {len(headers)} standalone headers, "
        f"{len(KERNEL_HEADERS) + len(KERNEL_SOURCES)} value-only kernel roots, "
        "diagnostics absent/enabled, no include cycles")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="clang", help="C compiler command")
    args = parser.parse_args()
    try:
        check(shlex.split(args.cc))
    except (OSError, RuntimeError, ValueError) as error:
        print(f"private headers: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
