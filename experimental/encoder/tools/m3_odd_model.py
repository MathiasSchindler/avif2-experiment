#!/usr/bin/env python3
"""Executable model and AVM probes for AV2 frames up to 16x16."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path

from m3_dc_model import choose_dc_level


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_AVMENC = ROOT / "build/reference/avm/avmenc"
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"
CASES = ((1, 1), (7, 9), (9, 7), (15, 15), (1, 16), (16, 1))


@dataclass(frozen=True)
class BlockVector:
    x: int
    y: int
    active_width: int
    active_height: int
    prediction: int
    padded_sum: int
    level: int
    reconstruction: int
    all_zero_context: int
    dc_sign_context: int


def coded_dimension(value: int) -> int:
    if not 1 <= value <= 16:
        raise ValueError("dimension must be in 1..16")
    return ((value + 7) // 8) * 8


def prediction_block(width: int, height: int) -> tuple[int, int]:
    return coded_dimension(width), coded_dimension(height)


def forced_partition_path(width: int, height: int) -> list[dict[str, object]]:
    coded_width, coded_height = prediction_block(width, height)
    path: list[dict[str, object]] = [
        {
            "block": "64x64", "action": "implied_HORZ",
            "result": "64x32", "coded_symbol": False,
        },
        {
            "block": "64x32", "action": "implied_VERT",
            "result": "32x32", "coded_symbol": False,
        },
        {
            "block": "32x32", "action": "implied_HORZ",
            "result": "32x16", "coded_symbol": False,
        },
        {
            "block": "32x16", "action": "implied_VERT",
            "result": "16x16", "coded_symbol": False,
        },
    ]
    if coded_width == 8 and coded_height == 8:
        path.extend(
            (
                {
                    "block": "16x16", "action": "implied_HORZ",
                    "result": "16x8", "coded_symbol": False,
                },
                {
                    "block": "16x8", "action": "implied_VERT",
                    "result": "8x8", "coded_symbol": False,
                },
                {
                    "block": "8x8", "action": "PARTITION_NONE",
                    "coded_symbol": True,
                    "do_split": 0,
                    "context": 0,
                },
            )
        )
    else:
        shape = f"{coded_width}x{coded_height}"
        if coded_width == 8:
            path.append(
                {
                    "block": "16x16", "action": "implied_VERT",
                    "result": shape, "coded_symbol": False,
                }
            )
        elif coded_height == 8:
            path.append(
                {
                    "block": "16x16", "action": "implied_HORZ",
                    "result": shape, "coded_symbol": False,
                }
            )
        path.append(
            {
                "block": shape,
                "action": "PARTITION_NONE",
                "coded_symbol": True,
                "do_split": 0,
                "context": 4,
            }
        )
    return path


def source_samples(width: int, height: int) -> bytes:
    values = (64, 160, 96, 208)
    return bytes(
        values[min(y // 8, 1) * 2 + min(x // 8, 1)]
        for y in range(height)
        for x in range(width)
    )


def edge_extend(samples: bytes, width: int, height: int) -> bytes:
    if len(samples) != width * height:
        raise ValueError("sample count does not match dimensions")
    coded_width, coded_height = prediction_block(width, height)
    return bytes(
        samples[min(y, height - 1) * width + min(x, width - 1)]
        for y in range(coded_height)
        for x in range(coded_width)
    )


def _context(levels: list[int], columns: int, row: int, column: int,
             block_equals_transform: bool) -> tuple[int, int]:
    neighbors = []
    if row:
        neighbors.append(levels[(row - 1) * columns + column])
    if column:
        neighbors.append(levels[row * columns + column - 1])
    skip = 0 if block_equals_transform else (
        sum(min(4, abs(level)) for level in neighbors) + 3
    ) >> 1
    score = sum(-1 if level < 0 else 1 if level > 0 else 0
                for level in neighbors)
    sign = 1 if score < 0 else 2 if score > 0 else 0
    return skip, sign


def model(width: int, height: int, qindex: int = 100) -> dict[str, object]:
    samples = source_samples(width, height)
    padded = edge_extend(samples, width, height)
    coded_width, coded_height = prediction_block(width, height)
    columns = coded_width // 8
    rows = coded_height // 8
    reconstruction = bytearray(coded_width * coded_height)
    levels: list[int] = []
    vectors: list[BlockVector] = []
    block_equals_transform = coded_width == coded_height == 8
    for row in range(rows):
        for column in range(columns):
            x, y = column * 8, row * 8
            above = (
                reconstruction[(y - 1) * coded_width + x:
                               (y - 1) * coded_width + x + 8]
                if row else None
            )
            left = (
                [reconstruction[(y + i) * coded_width + x - 1]
                 for i in range(8)]
                if column else None
            )
            edges = list(left or ()) + list(above or ())
            prediction = (
                128 if not edges else
                (sum(edges) + len(edges) // 2) // len(edges)
            )
            block = [
                padded[(y + iy) * coded_width + x + ix]
                for iy in range(8) for ix in range(8)
            ]
            vector = choose_dc_level(block, prediction, 8, qindex)
            skip, sign = _context(
                levels, columns, row, column, block_equals_transform
            )
            levels.append(vector.quantized_dc)
            for iy in range(8):
                start = (y + iy) * coded_width + x
                reconstruction[start:start + 8] = bytes(
                    [vector.reconstruction]
                ) * 8
            vectors.append(
                BlockVector(
                    x=x,
                    y=y,
                    active_width=min(8, width - x),
                    active_height=min(8, height - y),
                    prediction=prediction,
                    padded_sum=sum(block),
                    level=vector.quantized_dc,
                    reconstruction=vector.reconstruction,
                    all_zero_context=skip,
                    dc_sign_context=sign,
                )
            )
    cropped = b"".join(
        reconstruction[y * coded_width:y * coded_width + width]
        for y in range(height)
    )
    return {
        "width": width,
        "height": height,
        "coded_width": coded_width,
        "coded_height": coded_height,
        "prediction_block": f"{coded_width}x{coded_height}",
        "transform_size": "8x8",
        "dc_both_edge_divisor": 16,
        "partition_path": forced_partition_path(width, height),
        "source_sha256": hashlib.sha256(samples).hexdigest(),
        "padded_sha256": hashlib.sha256(padded).hexdigest(),
        "cropped_reconstruction_sha256": hashlib.sha256(cropped).hexdigest(),
        "blocks": [asdict(vector) for vector in vectors],
    }


def avm_command(avmenc: Path, width: int, height: int,
                source: Path, output: Path) -> list[str]:
    return [
        str(avmenc), "--codec=av2", "--obu", "--good", "--quiet", "--debug",
        "--passes=1", "--limit=1", f"--width={width}", f"--height={height}",
        "--bit-depth=8", "--input-bit-depth=8", "--monochrome",
        "--threads=1", "--row-mt=0", "--lag-in-frames=0", "--end-usage=q",
        "--qp=100", "--enable-tcq=0", "--enable-cdef=0",
        "--enable-restoration=0", "--enable-deblocking=0", "--enable-gdf=0",
        "--enable-qm=0", "--enable-palette=0", "--enable-intrabc=0",
        "--enable-ist=0", "--enable-fsc=0", "--enable-sdp=0",
        "--enable-angle-delta=0", "--enable-smooth-intra=0",
        "--enable-paeth-intra=0", "--enable-mrls=0", "--enable-intra-dip=0",
        "--enable-tx-partition=1", "--enable-flip-idtx=0",
        "-o", str(output), str(source),
    ]


def generate_avm(output: Path, avmenc: Path, avmdec: Path,
                 project_decoder: Path) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for width, height in CASES:
        name = f"{width}x{height}"
        y = source_samples(width, height)
        chroma_size = 2 * ((width + 1) // 2) * ((height + 1) // 2)
        source = output / f"{name}.i420"
        obu = output / f"{name}.obu"
        raw = output / f"{name}.i400"
        project_raw = output / f"{name}.project.i400"
        source.write_bytes(y + bytes([128]) * chroma_size)
        encoded = subprocess.run(
            avm_command(avmenc, width, height, source, obu),
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
        record: dict[str, object] = {
            "name": name,
            "width": width,
            "height": height,
            "accepted": encoded.returncode == 0,
            "encoder_returncode": encoded.returncode,
        }
        if encoded.returncode == 0:
            decoded = subprocess.run(
                [
                    str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
                    "--output-bit-depth=8", "-o", str(raw), str(obu),
                ],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            reconstructed = raw.read_bytes() if raw.is_file() else b""
            project = subprocess.run(
                [str(project_decoder), str(obu), "-o", str(project_raw)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            project_reconstructed = (
                project_raw.read_bytes() if project_raw.is_file() else b""
            )
            record.update(
                obu_bytes=obu.stat().st_size,
                obu_sha256=hashlib.sha256(obu.read_bytes()).hexdigest(),
                decoder_returncode=decoded.returncode,
                output_bytes=len(reconstructed),
                output_sha256=hashlib.sha256(reconstructed).hexdigest(),
                exact_visible_reconstruction=reconstructed == y,
                project_decoder_returncode=project.returncode,
                project_output_sha256=hashlib.sha256(
                    project_reconstructed
                ).hexdigest(),
                validators_exact_equal=project_reconstructed == reconstructed,
            )
        records.append(record)
    report = {
        "schema": "av2-m3-odd-avm-probes-v1",
        "avmenc_sha256": hashlib.sha256(avmenc.read_bytes()).hexdigest(),
        "avmdec_sha256": hashlib.sha256(avmdec.read_bytes()).hexdigest(),
        "project_decoder_sha256": hashlib.sha256(
            project_decoder.read_bytes()
        ).hexdigest(),
        "cases": records,
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n"
    )
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generate-avm", type=Path)
    parser.add_argument("--avmenc", type=Path, default=DEFAULT_AVMENC)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER
    )
    args = parser.parse_args()
    if args.generate_avm:
        report = generate_avm(
            args.generate_avm, args.avmenc, args.avmdec,
            args.project_decoder,
        )
    else:
        report = {
            "schema": "av2-m3-odd-model-v1",
            "qindex": 100,
            "cases": [model(width, height) for width, height in CASES],
        }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
