#!/usr/bin/env python3
"""Require the freestanding Windows binaries' exact import surfaces."""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


EXPECTED_DECODER = {
    "CloseHandle",
    "CreateFileW",
    "ExitProcess",
    "GetCommandLineW",
    "GetFileSizeEx",
    "GetStdHandle",
    "ReadFile",
    "VirtualAlloc",
    "VirtualFree",
    "WriteFile",
}
EXPECTED_SMOKE = {"ExitProcess"}


def imports(objdump: str, executable: Path) -> tuple[set[str], set[str]]:
    completed = subprocess.run(
        [objdump, "-p", str(executable)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    libraries = set(re.findall(r"DLL Name:\s*(\S+)", completed.stdout))
    symbols = set(
        re.findall(
            r"^\s+[0-9a-fA-F]+\s+<none>\s+[0-9a-fA-F]+\s+(\S+)\s*$",
            completed.stdout,
            re.MULTILINE,
        )
    )
    symbols.update(
        re.findall(
            r"^\s+\d+\s+([A-Za-z_?@]\S*)\s*$",
            completed.stdout,
            re.MULTILINE,
        )
    )
    return libraries, symbols


def check(
    objdump: str,
    executable: Path,
    expected_symbols: set[str],
) -> None:
    if not executable.is_file():
        raise RuntimeError(f"Windows executable does not exist: {executable}")
    libraries, symbols = imports(objdump, executable)
    if libraries != {"KERNEL32.dll"}:
        raise RuntimeError(
            f"{executable.name}: expected only KERNEL32.dll, got "
            f"{sorted(libraries)}"
        )
    if symbols != expected_symbols:
        raise RuntimeError(
            f"{executable.name}: imports {sorted(symbols)}, expected "
            f"{sorted(expected_symbols)}"
        )
    print(
        f"{executable.name}: KERNEL32.dll, "
        f"{len(symbols)} imports exact"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--objdump", default="objdump")
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--smoke", type=Path, required=True)
    args = parser.parse_args()
    check(args.objdump, args.decoder.resolve(), EXPECTED_DECODER)
    check(args.objdump, args.smoke.resolve(), EXPECTED_SMOKE)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
