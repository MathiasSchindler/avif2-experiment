#!/usr/bin/env python3
"""Generate pinned-AVM reference streams from locked prepared image inputs."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from obu_carriage import parse_obus


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IMAGES_ROOT = REPOSITORY_ROOT / "images"
PREPARED_ROOT = IMAGES_ROOT / "prepared"
REFERENCE_ROOT = IMAGES_ROOT / "reference"
PREPARED_LOCK_PATH = IMAGES_ROOT / "recipes" / "prepared-lock.json"
ENCODINGS_PATH = IMAGES_ROOT / "recipes" / "encodings.json"
AVM_ROOT = REPOSITORY_ROOT / "vendor" / "avm"
AVM_BUILD_ROOT = REPOSITORY_ROOT / "build" / "reference" / "avm"
EXPECTED_ENCODER_SHA256 = (
    "5894397c2a82382bcacf047f853dfd1d0543c75885f56922a77c5db95453110c"
)
EXPECTED_DECODER_SHA256 = (
    "afbec9f120cfdf61f84e656eb08db9b5ae62ceee29dd0ee3600b85c548d8d1a9"
)


def run(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        cwd=REPOSITORY_ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stderr.strip()}"
        )
    return completed.stdout


def sha256(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as input_file:
        while chunk := input_file.read(1024 * 1024):
            checksum.update(chunk)
    return checksum.hexdigest()


def file_record(path: Path) -> dict[str, object]:
    return {
        "filename": path.name,
        "byte_size": path.stat().st_size,
        "sha256": sha256(path),
    }


def expected_size(pixel_format: str, width: int, height: int) -> int:
    if pixel_format == "i400":
        return width * height
    if pixel_format == "i420":
        return width * height * 3 // 2
    if pixel_format == "i444":
        return width * height * 3
    raise ValueError(f"unsupported pixel format: {pixel_format}")


def normalize_command(command: list[str], scratch: Path) -> list[str]:
    normalized = []
    for argument in command:
        value = argument.replace(str(REPOSITORY_ROOT) + "/", "")
        value = value.replace(str(scratch) + "/", "")
        if value == argument and Path(value).is_absolute():
            value = Path(value).name
        normalized.append(value)
    return normalized


def normalize_obus(path: Path) -> list[dict[str, object]]:
    entries = parse_obus(path.read_bytes())
    normalized = [
        entry.encoded for entry in entries if entry.name != "content_interpretation"
    ]
    if len(normalized) != len(entries):
        path.write_bytes(b"".join(normalized))
        entries = parse_obus(path.read_bytes())
    names = [entry.name for entry in entries]
    if names != ["temporal_delimiter", "sequence_header", "closed_loop_key"]:
        raise RuntimeError(f"unexpected OBU ledger for {path}: {names}")
    return [entry.ledger_entry() for entry in entries]


def verify_tools(
    revision: str,
    expected_encoder_sha256: str,
    expected_decoder_sha256: str,
) -> tuple[Path, Path, dict[str, object]]:
    encoder = AVM_BUILD_ROOT / "avmenc"
    decoder = AVM_BUILD_ROOT / "avmdec"
    if not encoder.is_file() or not decoder.is_file():
        raise RuntimeError("run scripts/setup-avm.sh before generating references")
    actual_revision = run(["git", "-C", str(AVM_ROOT), "rev-parse", "HEAD"]).strip()
    if actual_revision != revision:
        raise RuntimeError(f"AVM revision is {actual_revision}, expected {revision}")
    encoder_hash = sha256(encoder)
    decoder_hash = sha256(decoder)
    if encoder_hash != expected_encoder_sha256:
        raise RuntimeError(f"unexpected avmenc SHA-256: {encoder_hash}")
    if decoder_hash != expected_decoder_sha256:
        raise RuntimeError(f"unexpected avmdec SHA-256: {decoder_hash}")
    source_changes = run(["git", "-C", str(AVM_ROOT), "status", "--short"])
    if source_changes.strip():
        raise RuntimeError("AVM source checkout is not clean")
    tools = {
        "revision": actual_revision,
        "encoder_sha256": encoder_hash,
        "decoder_sha256": decoder_hash,
    }
    return encoder, decoder, tools


def write_monochrome_y4m(source: Path, destination: Path, width: int, height: int) -> None:
    if source.stat().st_size != width * height:
        raise RuntimeError(f"unexpected I400 source size: {source}")
    header = f"YUV4MPEG2 W{width} H{height} F1:1 Ip A0:0 Cmono\nFRAME\n".encode(
        "ascii"
    )
    with destination.open("wb") as output, source.open("rb") as input_file:
        output.write(header)
        shutil.copyfileobj(input_file, output)


def encode_command(
    encoder: Path,
    suite: dict[str, object],
    source: Path,
    output: Path,
    pixel_format: str,
    width: int,
    height: int,
    qp: int,
    threads: int,
) -> list[str]:
    encoder_config = suite["encoder"]
    profile = "1" if pixel_format == "i444" else "0"
    format_arguments = {
        "i400": ["--monochrome"],
        "i420": [
            f"--width={width}",
            f"--height={height}",
            "--i420",
            "--input-chroma-subsampling-x=1",
            "--input-chroma-subsampling-y=1",
            "--chroma-sample-position=unspecified",
        ],
        "i444": [
            f"--width={width}",
            f"--height={height}",
            "--i444",
            "--input-chroma-subsampling-x=0",
            "--input-chroma-subsampling-y=0",
        ],
    }[pixel_format]
    return [
        str(encoder),
        "--debug",
        "--codec=av2",
        "--obu",
        "--passes=1",
        "--limit=1",
        "--good",
        f"--cpu-used={encoder_config['cpu_used']}",
        f"--threads={threads}",
        "--row-mt=1",
        f"--profile={profile}",
        "--bit-depth=8",
        "--input-bit-depth=8",
        *format_arguments,
        "--force-video-mode=0",
        "--film-grain-test=0",
        "--enable-lcr=0",
        "--enable-operating-point-sets=0",
        "--enable-atlas=0",
        *encoder_config["profile_arguments"],
        "--end-usage=q",
        f"--qp={qp}",
        "--test-decode=fatal",
        "--quiet",
        "-o",
        str(output),
        str(source),
    ]


def decode_command(decoder: Path, obu: Path, output: Path) -> list[str]:
    return [
        str(decoder),
        "--codec=av2",
        "--rawvideo",
        "--output-bit-depth=8",
        "--limit=1",
        "-o",
        str(output),
        str(obu),
    ]


def png_command(
    ffmpeg: str,
    source: Path,
    output: Path,
    pixel_format: str,
    width: int,
    height: int,
) -> list[str]:
    ffmpeg_format = {"i400": "gray", "i420": "yuv420p", "i444": "yuv444p"}[
        pixel_format
    ]
    return [
        ffmpeg,
        "-v",
        "error",
        "-f",
        "rawvideo",
        "-pixel_format",
        ffmpeg_format,
        "-video_size",
        f"{width}x{height}",
        "-i",
        str(source),
        "-frames:v",
        "1",
        "-pix_fmt",
        "rgb24",
        "-y",
        str(output),
    ]


def build_cases(
    suite: dict[str, object], prepared_lock: dict[str, object]
) -> list[dict[str, object]]:
    cases = []
    for preparation in prepared_lock["preparations"]:
        if preparation["preparation_id"] not in suite["preparation_ids"]:
            continue
        for qp in suite["baseline_qps"]:
            cases.append(
                {
                    "preparation": preparation,
                    "pixel_format": suite["baseline_pixel_format"],
                    "qp": qp,
                }
            )
    coverage = suite.get("format_coverage")
    if coverage is not None:
        coverage_preparations = [
            preparation
            for preparation in prepared_lock["preparations"]
            if preparation["preparation_id"] == coverage["preparation_id"]
        ]
        for preparation in coverage_preparations:
            for pixel_format in coverage["pixel_formats"]:
                for qp in coverage["qps"]:
                    cases.append(
                        {
                            "preparation": preparation,
                            "pixel_format": pixel_format,
                            "qp": qp,
                        }
                    )
    spectrum = suite.get("spectrum")
    if spectrum is not None:
        preparation = next(
            record
            for record in prepared_lock["preparations"]
            if record["source_id"] == spectrum["source_id"]
            and record["preparation_id"] == spectrum["preparation_id"]
        )
        for qp in spectrum["additional_qps"]:
            cases.append(
                {
                    "preparation": preparation,
                    "pixel_format": spectrum["pixel_format"],
                    "qp": qp,
                }
            )
    return sorted(
        cases,
        key=lambda case: (
            case["preparation"]["source_id"],
            case["preparation"]["preparation_id"],
            case["pixel_format"],
            case["qp"],
        ),
    )


def generate_case(
    case: dict[str, object],
    suite: dict[str, object],
    encoder: Path,
    decoder: Path,
    ffmpeg: str,
    threads: int,
    tools: dict[str, object],
    resume: bool,
) -> Path:
    preparation = case["preparation"]
    source_id = preparation["source_id"]
    preparation_id = preparation["preparation_id"]
    pixel_format = case["pixel_format"]
    qp = case["qp"]
    width, height = preparation["dimensions"]
    case_id = f"{source_id}-{preparation_id}-{pixel_format}-qp{qp:03d}"
    destination = (
        REFERENCE_ROOT
        / f"avm-{tools['revision']}"
        / suite["id"]
        / case_id
    )
    manifest_path = destination / "manifest.json"
    if resume and manifest_path.is_file():
        return destination

    source_output_name = {
        "i400": "gray8",
        "i420": "yuv420p8",
        "i444": "yuv444p8",
    }[pixel_format]
    prepared_manifest_path = IMAGES_ROOT / preparation["manifest"]
    source = (
        prepared_manifest_path.parent
        / preparation["outputs"][source_output_name]["filename"]
    )
    if sha256(source) != preparation["outputs"][source_output_name]["sha256"]:
        raise RuntimeError(f"prepared source hash mismatch: {source}")

    with tempfile.TemporaryDirectory(prefix=f"reference-{case_id}-") as temporary:
        scratch = Path(temporary)
        encoder_source = source
        if pixel_format == "i400":
            encoder_source = scratch / "source.y4m"
            write_monochrome_y4m(source, encoder_source, width, height)
        obu = scratch / "encoded.obu"
        decoded = scratch / f"decoded.{pixel_format}"
        encode = encode_command(
            encoder,
            suite,
            encoder_source,
            obu,
            pixel_format,
            width,
            height,
            qp,
            threads,
        )
        run(encode)
        obu_ledger = normalize_obus(obu)
        decode = decode_command(decoder, obu, decoded)
        run(decode)
        required_size = expected_size(pixel_format, width, height)
        if decoded.stat().st_size != required_size:
            raise RuntimeError(
                f"decoded size mismatch for {case_id}: "
                f"expected {required_size}, got {decoded.stat().st_size}"
            )

        outputs = {
            "encoded_obu": file_record(obu),
            "decoded": file_record(decoded),
        }
        commands = {
            "encode": normalize_command(encode, scratch),
            "decode": normalize_command(decode, scratch),
        }
        supplemental_outputs = suite["supplemental_outputs"]
        if supplemental_outputs["decoded_yuv_alias"]:
            yuv = scratch / "decoded.yuv"
            shutil.copyfile(decoded, yuv)
            outputs["decoded_yuv_alias"] = file_record(yuv)
        if supplemental_outputs["decoded_png"]:
            png = scratch / "decoded.png"
            convert = png_command(ffmpeg, decoded, png, pixel_format, width, height)
            run(convert)
            commands["decoded_png"] = normalize_command(convert, scratch)
            outputs["decoded_png"] = file_record(png)

        manifest = {
            "schema_version": 1,
            "case_id": case_id,
            "suite_id": suite["id"],
            "source_id": source_id,
            "preparation_id": preparation_id,
            "prepared_manifest_sha256": preparation["manifest_sha256"],
            "prepared_input": {
                "pixel_format": pixel_format,
                "filename": source.name,
                "sha256": preparation["outputs"][source_output_name]["sha256"],
            },
            "dimensions": [width, height],
            "bit_depth": 8,
            "range": "full",
            "matrix": "BT.709",
            "chroma_siting": "unspecified",
            "qp": qp,
            "encoder_threads": threads,
            "avm": tools,
            "commands": commands,
            "obu_ledger": obu_ledger,
            "outputs": outputs,
            "source_license": "inherited; see images/ATTRIBUTION.md",
        }
        (scratch / "manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(scratch, destination)
    return destination


def write_lock(
    destinations: list[Path],
    tools: dict[str, object],
    threads: int,
    jobs: int,
    lock_path: Path,
) -> None:
    records = []
    for destination in sorted(destinations):
        manifest_path = destination / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        records.append(
            {
                "manifest": str(manifest_path.relative_to(IMAGES_ROOT)),
                "manifest_sha256": sha256(manifest_path),
                "case_id": manifest["case_id"],
                "suite_id": manifest["suite_id"],
                "source_id": manifest["source_id"],
                "preparation_id": manifest["preparation_id"],
                "pixel_format": manifest["prepared_input"]["pixel_format"],
                "dimensions": manifest["dimensions"],
                "qp": manifest["qp"],
                "outputs": manifest["outputs"],
            }
        )
    lock = {
        "schema_version": 1,
        "generated_by": "tools/generate_image_references.py",
        "avm": tools,
        "encoder_threads": threads,
        "generation_jobs": jobs,
        "references": records,
    }
    lock_path.write_text(
        json.dumps(lock, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--suite", help="encoding suite ID (defaults to the first suite)")
    parser.add_argument("--source", action="append", help="generate only this source ID")
    parser.add_argument("--preparation", action="append", help="generate only this preparation ID")
    parser.add_argument("--pixel-format", action="append", choices=("i400", "i420", "i444"))
    parser.add_argument("--qp", action="append", type=int, help="generate only this QP")
    parser.add_argument("--threads", type=int)
    parser.add_argument(
        "--jobs",
        type=int,
        help="number of independent AVM cases to encode concurrently (suite default if omitted)",
    )
    parser.add_argument("--resume", action="store_true")
    arguments = parser.parse_args()
    encodings = json.loads(ENCODINGS_PATH.read_text(encoding="utf-8"))
    suite_id = arguments.suite or encodings["suites"][0]["id"]
    try:
        suite = next(suite for suite in encodings["suites"] if suite["id"] == suite_id)
    except StopIteration as error:
        raise SystemExit(f"unknown encoding suite: {suite_id}") from error
    lock_path = IMAGES_ROOT / "recipes" / suite["lock_file"]
    if arguments.threads is None:
        arguments.threads = suite["encoder"].get("threads", os.cpu_count() or 1)
    if arguments.jobs is None:
        arguments.jobs = suite["encoder"]["parallel_jobs"]
    if arguments.threads < 1:
        raise SystemExit("--threads must be positive")
    if arguments.jobs < 1:
        raise SystemExit("--jobs must be positive")
    if arguments.qp and any(qp < 0 or qp > 255 for qp in arguments.qp):
        raise SystemExit("--qp values must be in the range 0..255")
    prepared_lock = json.loads(PREPARED_LOCK_PATH.read_text(encoding="utf-8"))
    cases = build_cases(suite, prepared_lock)
    filters = {
        "source": set(arguments.source or []),
        "preparation": set(arguments.preparation or []),
        "pixel_format": set(arguments.pixel_format or []),
        "qp": set(arguments.qp or []),
    }
    cases = [
        case
        for case in cases
        if (not filters["source"] or case["preparation"]["source_id"] in filters["source"])
        and (
            not filters["preparation"]
            or case["preparation"]["preparation_id"] in filters["preparation"]
        )
        and (not filters["pixel_format"] or case["pixel_format"] in filters["pixel_format"])
        and (not filters["qp"] or case["qp"] in filters["qp"])
    ]
    if not cases:
        raise SystemExit("no reference cases match the requested filters")

    binary_sha256 = suite.get("avm_binary_sha256", {})
    encoder, decoder, tools = verify_tools(
        encodings["avm_revision"],
        binary_sha256.get("encoder", EXPECTED_ENCODER_SHA256),
        binary_sha256.get("decoder", EXPECTED_DECODER_SHA256),
    )
    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise RuntimeError("required tool not found: ffmpeg")
    destinations = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=arguments.jobs) as pool:
        pending = {
            pool.submit(
                generate_case,
                case,
                suite,
                encoder,
                decoder,
                ffmpeg,
                arguments.threads,
                tools,
                arguments.resume,
            ): case
            for case in cases
        }
        for index, future in enumerate(
            concurrent.futures.as_completed(pending), start=1
        ):
            case = pending[future]
            preparation = case["preparation"]
            label = (
                f"{preparation['source_id']}-{preparation['preparation_id']}-"
                f"{case['pixel_format']}-qp{case['qp']:03d}"
            )
            destinations.append(future.result())
            print(f"[{index:03d}/{len(cases):03d}] {label}", flush=True)
    if not any((arguments.source, arguments.preparation, arguments.pixel_format, arguments.qp)):
        write_lock(destinations, tools, arguments.threads, arguments.jobs, lock_path)
        print(lock_path.relative_to(REPOSITORY_ROOT), flush=True)
    print(
        f"generated {len(destinations)} reference cases with "
        f"{arguments.jobs} jobs and {arguments.threads} threads per AVM invocation"
    )


if __name__ == "__main__":
    main()