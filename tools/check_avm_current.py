#!/usr/bin/env python3
"""Compare a moving AVM oracle with immutable v1.0.0 references and fresh encodes."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import TypedDict

from generate_image_references import file_record, run, sha256, write_monochrome_y4m
from obu_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
IMAGES = ROOT / "images"
PINNED_REVISION = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
SUFFIX = ".exe" if os.name == "nt" else ""


class CommandResult(TypedDict, total=False):
    status: str
    command: list[str]
    returncode: int
    stderr: str
    error: str
    output: dict[str, object]
    comparable_sha256: str
    matches_baseline: bool


class CaseResult(TypedDict, total=False):
    suite: str
    case_id: str
    pixel_format: str
    baseline_encoded_sha256: str
    baseline_decoded_sha256: str
    source_sha256: str
    status: str
    reason: str
    error: str
    existing_decode: CommandResult
    fresh_encode: CommandResult
    fresh_oracle_decode: CommandResult
    fresh_project_decode: CommandResult


@dataclass(frozen=True)
class Case:
    suite: str
    name: str
    source_id: str
    preparation: str
    pixel_format: str
    width: int
    height: int
    encoded: Path
    encoded_sha256: str
    decoded_sha256: str
    decoded_size: int
    source: Path
    source_sha256: str
    encode: tuple[str, ...]

    @property
    def bit_depth(self) -> int:
        return 10 if self.pixel_format.endswith("10le") else 8


def verify_file(path: Path, expected: str) -> None:
    if sha256(path) != expected:
        raise ValueError(f"SHA-256 mismatch: {path}")


def photo_cases(suite_id: str, lock_file: str) -> list[Case]:
    lock = json.loads((IMAGES / "recipes" / lock_file).read_text())
    if lock["avm"]["revision"] != PINNED_REVISION:
        raise ValueError(f"{suite_id}: reference is not AVM v1.0.0")
    cases = []
    for record in lock["references"]:
        manifest_path = IMAGES / record["manifest"]
        encoded = record["outputs"]["encoded_obu"]
        decoded = record["outputs"]["decoded"]
        prepared_path = (
            IMAGES / "prepared" / record["preparation_id"] / record["source_id"]
        )
        # A missing ignored manifest must remain visible in the report.
        command = ()
        source = prepared_path / "manifest.json"
        source_hash = ""
        if manifest_path.is_file():
            verify_file(manifest_path, record["manifest_sha256"])
            manifest = json.loads(manifest_path.read_text())
            command = tuple(manifest["commands"]["encode"])
            source = prepared_path / manifest["prepared_input"]["filename"]
            source_hash = manifest["prepared_input"]["sha256"]
        cases.append(Case(
            suite_id, record["case_id"], record["source_id"],
            record["preparation_id"], record["pixel_format"],
            *record["dimensions"], manifest_path.parent / encoded["filename"],
            encoded["sha256"], decoded["sha256"], decoded["byte_size"],
            source, source_hash, command,
        ))
    return cases


def ten_bit_cases() -> list[Case]:
    directory = ROOT / "tests" / "fixtures" / "10bit" / "avm-reference"
    manifest = json.loads((directory / "manifest.json").read_text())
    if manifest["tools"]["avm_revision"] != PINNED_REVISION:
        raise ValueError("10-bit reference is not AVM v1.0.0")
    cases = []
    for record in manifest["artifacts"]:
        cases.append(Case(
            "10bit", record["name"], record["name"], "synthetic",
            record["pixel_format"], record["width"], record["height"],
            directory / record["obu"]["file"], record["obu"]["sha256"],
            record["decoded"]["sha256"], record["decoded"]["size_bytes"],
            directory.parent / record["source"], record["source_sha256"],
            tuple(record["encode_command"]),
        ))
    return cases


def replay_encode(
    command: tuple[str, ...], encoder: Path, source: Path, output: Path
) -> list[str]:
    if command.count("-o") != 1 or len(command) < 4:
        raise ValueError("reference encode command must contain one -o argument")
    result = list(command)
    result[0] = str(encoder)
    result[result.index("-o") + 1] = str(output)
    result[-1] = str(source)
    return result


def decode_command(decoder: Path, case: Case, source: Path, output: Path) -> list[str]:
    return [
        str(decoder), "--codec=av2", "--rawvideo",
        f"--output-bit-depth={case.bit_depth}", "--limit=1", "--threads=1",
        "-o", str(output), str(source),
    ]


def execute(command: list[str], output: Path, size: int | None = None) -> CommandResult:
    completed = subprocess.run(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    result: CommandResult = {
        "command": command, "returncode": completed.returncode,
        "stderr": completed.stderr, "status": "error",
    }
    if completed.returncode == 0:
        if not output.is_file():
            result["error"] = "command succeeded without creating output"
        else:
            result["output"] = file_record(output)
            if output.stat().st_size == 0:
                result["error"] = "command created empty output"
            elif size is not None and output.stat().st_size != size:
                result["error"] = f"expected {size} bytes, got {output.stat().st_size}"
            else:
                result["status"] = "ok"
    return result


def check_case(
    case: Case, destination: Path, encoder: Path, oracle: Path, decoder: Path,
    mode: str, allow_missing: bool,
) -> CaseResult:
    result: CaseResult = {
        "suite": case.suite, "case_id": case.name, "pixel_format": case.pixel_format,
        "baseline_encoded_sha256": case.encoded_sha256,
        "baseline_decoded_sha256": case.decoded_sha256,
        "source_sha256": case.source_sha256, "status": "error",
    }
    try:
        if not case.encoded.is_file() and allow_missing:
            result.update(status="skipped", reason=f"missing reference: {case.encoded}")
            return result
        verify_file(case.encoded, case.encoded_sha256)
        destination.mkdir(parents=True)
        failed = False
        if mode in ("decode", "both"):
            output = destination / f"existing.{case.pixel_format}"
            check = execute(
                decode_command(oracle, case, case.encoded, output),
                output, case.decoded_size,
            )
            if check["status"] == "ok":
                check["status"] = (
                    "exact" if sha256(output) == case.decoded_sha256 else "mismatch"
                )
            failed = check["status"] != "exact"
            result["existing_decode"] = check
        if mode in ("encode", "both"):
            if not case.source.is_file() and allow_missing:
                result["fresh_encode"] = {
                    "status": "skipped", "reason": f"missing prepared input: {case.source}",
                }
                result["status"] = "error" if failed else "partial"
                return result
            verify_file(case.source, case.source_sha256)
            source = case.source
            if case.pixel_format == "i400":
                source = destination / "source.y4m"
                write_monochrome_y4m(case.source, source, case.width, case.height)
            encoded = destination / "encoded.obu"
            encode = execute(replay_encode(case.encode, encoder, source, encoded), encoded)
            result["fresh_encode"] = encode
            if encode["status"] != "ok":
                return result
            comparable = b"".join(
                obu.encoded for obu in parse_obus(encoded.read_bytes())
                if obu.name != "content_interpretation"
            )
            digest = hashlib.sha256(comparable).hexdigest()
            encode["comparable_sha256"] = digest
            encode["status"] = (
                "identical" if digest == case.encoded_sha256 else "changed"
            )
            output = destination / f"oracle.{case.pixel_format}"
            check = execute(
                decode_command(oracle, case, encoded, output),
                output, case.decoded_size,
            )
            result["fresh_oracle_decode"] = check
            if check["status"] != "ok":
                return result
            digest = sha256(output)
            check["matches_baseline"] = digest == case.decoded_sha256
            project_output = destination / f"project.{case.pixel_format}"
            project = execute(
                [str(decoder), str(encoded), "-o", str(project_output)],
                project_output, case.decoded_size,
            )
            if project["status"] == "ok":
                project["status"] = (
                    "exact" if sha256(project_output) == digest else "mismatch"
                )
            result["fresh_project_decode"] = project
            failed |= project["status"] != "exact"
        result["status"] = "error" if failed else "passed"
    except (OSError, ValueError) as error:
        result["error"] = str(error)
    return result


def oracle_tools(source: Path, build: Path) -> tuple[Path, Path, dict[str, object]]:
    revision = run(["git", "-C", str(source), "rev-parse", "HEAD"]).strip()
    if run(["git", "-C", str(source), "status", "--short"]).strip():
        raise ValueError(f"current AVM checkout has local modifications: {source}")
    if (build / "oracle-revision.txt").read_text().strip() != revision:
        raise ValueError("current AVM build is stale; rerun reference-avm-current")
    encoder, decoder = build / f"avmenc{SUFFIX}", build / f"avmdec{SUFFIX}"
    tools = {
        "source_url": "https://github.com/AOMediaCodec/avm.git",
        "branch": "main", "revision": revision,
        "encoder": file_record(encoder), "decoder": file_record(decoder),
    }
    return encoder, decoder, tools


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--avm-source", type=Path, default=ROOT / "vendor" / "avm-current")
    parser.add_argument("--avm-build", type=Path, default=ROOT / "build/reference/avm-current")
    parser.add_argument("--decoder", type=Path, default=ROOT / f"build/host-sanitized/av2heif-decoder{SUFFIX}")
    parser.add_argument("--suite", action="append", help="repeatable suite ID, or 10bit; default: all generated suites and 10bit")
    parser.add_argument("--source", action="append", help="repeatable photographic source ID or 10-bit case name")
    parser.add_argument("--preparation", action="append")
    parser.add_argument("--pixel-format", action="append")
    parser.add_argument("--mode", choices=("decode", "encode", "both"), default="both")
    parser.add_argument("--allow-missing", action="store_true", help="explicitly report missing local media as skipped")
    parser.add_argument("--output-dir", type=Path, help="new directory; existing directories are never overwritten")
    args = parser.parse_args()
    encoder, oracle, tools = oracle_tools(args.avm_source.resolve(), args.avm_build.resolve())
    decoder = args.decoder.resolve()
    if args.mode != "decode" and not decoder.is_file():
        parser.error(f"project decoder does not exist: {decoder}")
    encodings = json.loads((IMAGES / "recipes/encodings.json").read_text())
    suites = {
        suite["id"]: suite for suite in encodings["suites"]
        if suite.get("reference_status", "generated") == "generated"
    }
    selected = args.suite or [*suites, "10bit"]
    if set(selected) - (set(suites) | {"10bit"}):
        parser.error("unknown or ungenerated suite")
    cases = []
    for identifier in dict.fromkeys(selected):
        cases.extend(
            ten_bit_cases() if identifier == "10bit"
            else photo_cases(identifier, suites[identifier]["lock_file"])
        )
    cases = [
        case for case in cases
        if (not args.source or case.source_id in args.source)
        and (not args.preparation or case.preparation in args.preparation)
        and (not args.pixel_format or case.pixel_format in args.pixel_format)
    ]
    if not cases:
        parser.error("no cases match the requested filters")
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    destination = (
        args.output_dir or ROOT / "build/oracle-current" / timestamp
    ).resolve()
    destination.mkdir(parents=True, exist_ok=False)
    report: dict[str, object] = {
        "schema": "av2h-current-oracle-v1", "baseline_revision": PINNED_REVISION,
        "current_avm": tools, "mode": args.mode, "allow_missing": args.allow_missing,
        "project_decoder": file_record(decoder) if args.mode != "decode" else None,
        "requested_cases": len(cases),
    }
    results = []
    report_path = destination / "report.json"
    counts = {}
    for case in cases:
        result = check_case(
            case, destination / case.suite / case.name,
            encoder, oracle, decoder, args.mode, args.allow_missing,
        )
        results.append(result)
        print(f"{case.suite}/{case.name}: {result['status']}", flush=True)
        if result["status"] != "passed":
            print(json.dumps(result), file=sys.stderr, flush=True)
        counts = {
            status: sum(result["status"] == status for result in results)
            for status in ("passed", "error", "skipped", "partial")
        }
        report.update(
            cases=results, summary=counts, complete=len(results) == len(cases),
        )
        temporary = report_path.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        temporary.replace(report_path)
    print(f"{counts}\nReport: {report_path}", flush=True)
    return 1 if counts["error"] or not counts["passed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
